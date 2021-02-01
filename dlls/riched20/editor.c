/*
 * RichEdit - functions dealing with editor object
 *
 * Copyright 2004 by Krzysztof Foltman
 * Copyright 2005 by Cihan Altinay
 * Copyright 2005 by Phil Krylov
 * Copyright 2008 Eric Pouech
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/* 
  API implementation status:
  
  Messages (ANSI versions not done yet)
  + EM_AUTOURLDETECT 2.0
  + EM_CANPASTE
  + EM_CANREDO 2.0
  + EM_CANUNDO
  + EM_CHARFROMPOS
  - EM_DISPLAYBAND
  + EM_EMPTYUNDOBUFFER
  + EM_EXGETSEL
  + EM_EXLIMITTEXT
  + EM_EXLINEFROMCHAR
  + EM_EXSETSEL
  + EM_FINDTEXT (only FR_DOWN flag implemented)
  + EM_FINDTEXTEX (only FR_DOWN flag implemented)
  - EM_FINDWORDBREAK
  - EM_FMTLINES
  - EM_FORMATRANGE
  + EM_GETAUTOURLDETECT 2.0
  - EM_GETBIDIOPTIONS 3.0
  - EM_GETCHARFORMAT (partly done)
  - EM_GETEDITSTYLE
  + EM_GETEVENTMASK
  + EM_GETFIRSTVISIBLELINE (can be optimized if needed)
  - EM_GETIMECOLOR 1.0asian
  - EM_GETIMECOMPMODE 2.0
  - EM_GETIMEOPTIONS 1.0asian
  - EM_GETIMESTATUS
  - EM_GETLANGOPTIONS 2.0
  + EM_GETLIMITTEXT
  + EM_GETLINE
  + EM_GETLINECOUNT   returns number of rows, not of paragraphs
  + EM_GETMODIFY
  + EM_GETOLEINTERFACE
  + EM_GETOPTIONS
  + EM_GETPARAFORMAT
  + EM_GETPASSWORDCHAR 2.0
  - EM_GETPUNCTUATION 1.0asian
  + EM_GETRECT
  - EM_GETREDONAME 2.0
  + EM_GETSEL
  + EM_GETSELTEXT (ANSI&Unicode)
  + EM_GETSCROLLPOS 3.0
! - EM_GETTHUMB
  + EM_GETTEXTEX 2.0
  + EM_GETTEXTLENGTHEX (GTL_PRECISE unimplemented)
  + EM_GETTEXTMODE 2.0
? + EM_GETTEXTRANGE (ANSI&Unicode)
  - EM_GETTYPOGRAPHYOPTIONS 3.0
  - EM_GETUNDONAME
  + EM_GETWORDBREAKPROC
  - EM_GETWORDBREAKPROCEX
  - EM_GETWORDWRAPMODE 1.0asian
  + EM_GETZOOM 3.0
  + EM_HIDESELECTION
  + EM_LIMITTEXT (Also called EM_SETLIMITTEXT)
  + EM_LINEFROMCHAR
  + EM_LINEINDEX
  + EM_LINELENGTH
  + EM_LINESCROLL
  - EM_PASTESPECIAL
  + EM_POSFROMCHAR
  + EM_REDO 2.0
  + EM_REQUESTRESIZE
  + EM_REPLACESEL (proper style?) ANSI&Unicode
  + EM_SCROLL
  + EM_SCROLLCARET
  + EM_SELECTIONTYPE
  - EM_SETBIDIOPTIONS 3.0
  + EM_SETBKGNDCOLOR
  + EM_SETCHARFORMAT (partly done, no ANSI)
  - EM_SETEDITSTYLE
  + EM_SETEVENTMASK (few notifications supported)
  + EM_SETFONTSIZE
  - EM_SETIMECOLOR 1.0asian
  - EM_SETIMEOPTIONS 1.0asian
  - EM_SETIMESTATUS
  - EM_SETLANGOPTIONS 2.0
  - EM_SETLIMITTEXT
  - EM_SETMARGINS
  + EM_SETMODIFY (not sure if implementation is correct)
  - EM_SETOLECALLBACK
  + EM_SETOPTIONS (partially implemented)
  - EM_SETPALETTE 2.0
  + EM_SETPARAFORMAT
  + EM_SETPASSWORDCHAR 2.0
  - EM_SETPUNCTUATION 1.0asian
  + EM_SETREADONLY no beep on modification attempt
  + EM_SETRECT
  + EM_SETRECTNP (EM_SETRECT without repainting)
  + EM_SETSEL
  + EM_SETSCROLLPOS 3.0
  - EM_SETTABSTOPS 3.0
  - EM_SETTARGETDEVICE (partial)
  + EM_SETTEXTEX 3.0 (proper style?)
  - EM_SETTEXTMODE 2.0
  - EM_SETTYPOGRAPHYOPTIONS 3.0
  + EM_SETUNDOLIMIT 2.0
  + EM_SETWORDBREAKPROC (used only for word movement at the moment)
  - EM_SETWORDBREAKPROCEX
  - EM_SETWORDWRAPMODE 1.0asian
  + EM_SETZOOM 3.0
  + EM_SHOWSCROLLBAR 2.0
  + EM_STOPGROUPTYPING 2.0
  + EM_STREAMIN
  + EM_STREAMOUT
  + EM_UNDO
  + WM_CHAR
  + WM_CLEAR
  + WM_COPY
  + WM_CUT
  + WM_GETDLGCODE (the current implementation is incomplete)
  + WM_GETTEXT (ANSI&Unicode)
  + WM_GETTEXTLENGTH (ANSI version sucks)
  + WM_HSCROLL
  + WM_PASTE
  + WM_SETFONT
  + WM_SETTEXT (resets undo stack !) (proper style?) ANSI&Unicode
  + WM_STYLECHANGING (seems to do nothing)
  + WM_STYLECHANGED (seems to do nothing)
  + WM_UNICHAR
  + WM_VSCROLL

  Notifications

  * EN_CHANGE (sent from the wrong place)
  - EN_CORRECTTEXT
  - EN_DROPFILES
  - EN_ERRSPACE
  - EN_HSCROLL
  - EN_IMECHANGE
  + EN_KILLFOCUS
  - EN_LINK
  - EN_MAXTEXT
  - EN_MSGFILTER
  - EN_OLEOPFAILED
  - EN_PROTECTED
  + EN_REQUESTRESIZE
  - EN_SAVECLIPBOARD
  + EN_SELCHANGE 
  + EN_SETFOCUS
  - EN_STOPNOUNDO
  * EN_UPDATE (sent from the wrong place)
  - EN_VSCROLL
  
  Styles
  
  - ES_AUTOHSCROLL
  - ES_AUTOVSCROLL
  + ES_CENTER
  + ES_DISABLENOSCROLL (scrollbar is always visible)
  - ES_EX_NOCALLOLEINIT
  + ES_LEFT
  + ES_MULTILINE
  - ES_NOIME
  - ES_READONLY (I'm not sure if beeping is the proper behaviour)
  + ES_RIGHT
  - ES_SAVESEL
  - ES_SELFIME
  - ES_SUNKEN
  - ES_VERTICAL
  - ES_WANTRETURN (don't know how to do WM_GETDLGCODE part)
  - WS_SETFONT
  + WS_HSCROLL
  + WS_VSCROLL
*/

/*
 * RICHED20 TODO (incomplete):
 *
 * - messages/styles/notifications listed above 
 * - add remaining CHARFORMAT/PARAFORMAT fields
 * - right/center align should strip spaces from the beginning
 * - pictures/OLE objects (not just smiling faces that lack API support ;-) )
 * - COM interface (looks like a major pain in the TODO list)
 * - calculate heights of pictures (half-done)
 * - hysteresis during wrapping (related to scrollbars appearing/disappearing)
 * - find/replace
 * - how to implement EM_FORMATRANGE and EM_DISPLAYBAND ? (Mission Impossible)
 * - italic caret with italic fonts
 * - IME
 * - most notifications aren't sent at all (the most important ones are)
 * - when should EN_SELCHANGE be sent after text change ? (before/after EN_UPDATE?)
 * - WM_SETTEXT may use wrong style (but I'm 80% sure it's OK)
 * - EM_GETCHARFORMAT with SCF_SELECTION may not behave 100% like in original (but very close)
 * - full justification
 * - hyphenation
 * - tables
 * - ListBox & ComboBox not implemented
 *
 * Bugs that are probably fixed, but not so easy to verify:
 * - EN_UPDATE/EN_CHANGE are handled very incorrectly (should be OK now)
 * - undo for ME_JoinParagraphs doesn't store paragraph format ? (it does)
 * - check/fix artificial EOL logic (bCursorAtEnd, hardly logical)
 * - caret shouldn't be displayed when selection isn't empty
 * - check refcounting in style management functions (looks perfect now, but no bugs is suspicious)
 * - undo for setting default format (done, might be buggy)
 * - styles might be not released properly (looks like they work like charm, but who knows?
 *
 */

#define NONAMELESSUNION

#include "editor.h"
#include "commdlg.h"
#include "winreg.h"
#define NO_SHLWAPI_STREAM 
#include "shlwapi.h"
#include "rtf.h"
#include "imm.h"
#include "res.h"

#define STACK_SIZE_DEFAULT  100
#define STACK_SIZE_MAX     1000

#define TEXT_LIMIT_DEFAULT 32767
 
WINE_DEFAULT_DEBUG_CHANNEL(richedit);

static BOOL ME_RegisterEditorClass(HINSTANCE);
static BOOL ME_UpdateLinkAttribute(ME_TextEditor *editor, ME_Cursor *start, int nChars);
static HCURSOR hLeft;

BOOL me_debug = FALSE;
HANDLE me_heap = NULL;

static BOOL ME_ListBoxRegistered = FALSE;
static BOOL ME_ComboBoxRegistered = FALSE;

static inline BOOL is_version_nt(void)
{
    return !(GetVersion() & 0x80000000);
}

static ME_TextBuffer *ME_MakeText(void) {
  ME_TextBuffer *buf = heap_alloc(sizeof(*buf));
  ME_DisplayItem *p1 = ME_MakeDI(diTextStart);
  ME_DisplayItem *p2 = ME_MakeDI(diTextEnd);
  
  p1->prev = NULL;
  p1->next = p2;
  p2->prev = p1;
  p2->next = NULL;
  p1->member.para.next_para = p2;
  p2->member.para.prev_para = p1;
  p2->member.para.nCharOfs = 0;  
  
  buf->pFirst = p1;
  buf->pLast = p2;
  buf->pCharStyle = NULL;
  
  return buf;
}

ME_Paragraph *editor_first_para( ME_TextEditor *editor )
{
    return para_next( &editor->pBuffer->pFirst->member.para );
}

/* Note, returns the diTextEnd sentinel paragraph */
ME_Paragraph *editor_end_para( ME_TextEditor *editor )
{
    return &editor->pBuffer->pLast->member.para;
}

static LRESULT ME_StreamInText(ME_TextEditor *editor, DWORD dwFormat, ME_InStream *stream, ME_Style *style)
{
  WCHAR *pText;
  LRESULT total_bytes_read = 0;
  BOOL is_read = FALSE;
  DWORD cp = CP_ACP, copy = 0;
  char conv_buf[4 + STREAMIN_BUFFER_SIZE]; /* up to 4 additional UTF-8 bytes */

  static const char bom_utf8[] = {0xEF, 0xBB, 0xBF};

  TRACE("%08x %p\n", dwFormat, stream);

  do {
    LONG nWideChars = 0;
    WCHAR wszText[STREAMIN_BUFFER_SIZE+1];

    if (!stream->dwSize)
    {
      ME_StreamInFill(stream);
      if (stream->editstream->dwError)
        break;
      if (!stream->dwSize)
        break;
      total_bytes_read += stream->dwSize;
    }

    if (!(dwFormat & SF_UNICODE))
    {
      char * buf = stream->buffer;
      DWORD size = stream->dwSize, end;

      if (!is_read)
      {
        is_read = TRUE;
        if (stream->dwSize >= 3 && !memcmp(stream->buffer, bom_utf8, 3))
        {
          cp = CP_UTF8;
          buf += 3;
          size -= 3;
        }
      }

      if (cp == CP_UTF8)
      {
        if (copy)
        {
          memcpy(conv_buf + copy, buf, size);
          buf = conv_buf;
          size += copy;
        }
        end = size;
        while ((buf[end-1] & 0xC0) == 0x80)
        {
          --end;
          --total_bytes_read; /* strange, but seems to match windows */
        }
        if (buf[end-1] & 0x80)
        {
          DWORD need = 0;
          if ((buf[end-1] & 0xE0) == 0xC0)
            need = 1;
          if ((buf[end-1] & 0xF0) == 0xE0)
            need = 2;
          if ((buf[end-1] & 0xF8) == 0xF0)
            need = 3;

          if (size - end >= need)
          {
            /* we have enough bytes for this sequence */
            end = size;
          }
          else
          {
            /* need more bytes, so don't transcode this sequence */
            --end;
          }
        }
      }
      else
        end = size;

      nWideChars = MultiByteToWideChar(cp, 0, buf, end, wszText, STREAMIN_BUFFER_SIZE);
      pText = wszText;

      if (cp == CP_UTF8)
      {
        if (end != size)
        {
          memcpy(conv_buf, buf + end, size - end);
          copy = size - end;
        }
      }
    }
    else
    {
      nWideChars = stream->dwSize >> 1;
      pText = (WCHAR *)stream->buffer;
    }

    ME_InsertTextFromCursor(editor, 0, pText, nWideChars, style);
    if (stream->dwSize == 0)
      break;
    stream->dwSize = 0;
  } while(1);
  return total_bytes_read;
}

static void ME_ApplyBorderProperties(RTF_Info *info,
                                     ME_BorderRect *borderRect,
                                     RTFBorder *borderDef)
{
  int i, colorNum;
  ME_Border *pBorders[] = {&borderRect->top,
                           &borderRect->left,
                           &borderRect->bottom,
                           &borderRect->right};
  for (i = 0; i < 4; i++)
  {
    RTFColor *colorDef = info->colorList;
    pBorders[i]->width = borderDef[i].width;
    colorNum = borderDef[i].color;
    while (colorDef && colorDef->rtfCNum != colorNum)
      colorDef = colorDef->rtfNextColor;
    if (colorDef)
      pBorders[i]->colorRef = RGB(
                           colorDef->rtfCRed >= 0 ? colorDef->rtfCRed : 0,
                           colorDef->rtfCGreen >= 0 ? colorDef->rtfCGreen : 0,
                           colorDef->rtfCBlue >= 0 ? colorDef->rtfCBlue : 0);
    else
      pBorders[i]->colorRef = RGB(0, 0, 0);
  }
}

void ME_RTFCharAttrHook(RTF_Info *info)
{
  CHARFORMAT2W fmt;
  fmt.cbSize = sizeof(fmt);
  fmt.dwMask = 0;
  fmt.dwEffects = 0;

  switch(info->rtfMinor)
  {
    case rtfPlain:
      /* FIXME add more flags once they're implemented */
      fmt.dwMask = CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_UNDERLINETYPE | CFM_STRIKEOUT |
          CFM_COLOR | CFM_BACKCOLOR | CFM_SIZE | CFM_WEIGHT;
      fmt.dwEffects = CFE_AUTOCOLOR | CFE_AUTOBACKCOLOR;
      fmt.yHeight = 12*20; /* 12pt */
      fmt.wWeight = FW_NORMAL;
      fmt.bUnderlineType = CFU_UNDERLINE;
      break;
    case rtfBold:
      fmt.dwMask = CFM_BOLD | CFM_WEIGHT;
      fmt.dwEffects = info->rtfParam ? CFE_BOLD : 0;
      fmt.wWeight = info->rtfParam ? FW_BOLD : FW_NORMAL;
      break;
    case rtfItalic:
      fmt.dwMask = CFM_ITALIC;
      fmt.dwEffects = info->rtfParam ? fmt.dwMask : 0;
      break;
    case rtfUnderline:
      fmt.dwMask = CFM_UNDERLINETYPE | CFM_UNDERLINE;
      fmt.bUnderlineType = CFU_UNDERLINE;
      fmt.dwEffects = info->rtfParam ? CFE_UNDERLINE : 0;
      break;
    case rtfDotUnderline:
      fmt.dwMask = CFM_UNDERLINETYPE | CFM_UNDERLINE;
      fmt.bUnderlineType = CFU_UNDERLINEDOTTED;
      fmt.dwEffects = info->rtfParam ? CFE_UNDERLINE : 0;
      break;
    case rtfDbUnderline:
      fmt.dwMask = CFM_UNDERLINETYPE | CFM_UNDERLINE;
      fmt.bUnderlineType = CFU_UNDERLINEDOUBLE;
      fmt.dwEffects = info->rtfParam ? CFE_UNDERLINE : 0;
      break;
    case rtfWordUnderline:
      fmt.dwMask = CFM_UNDERLINETYPE | CFM_UNDERLINE;
      fmt.bUnderlineType = CFU_UNDERLINEWORD;
      fmt.dwEffects = info->rtfParam ? CFE_UNDERLINE : 0;
      break;
    case rtfNoUnderline:
      fmt.dwMask = CFM_UNDERLINE;
      fmt.dwEffects = 0;
      break;
    case rtfStrikeThru:
      fmt.dwMask = CFM_STRIKEOUT;
      fmt.dwEffects = info->rtfParam ? fmt.dwMask : 0;
      break;
    case rtfSubScript:
    case rtfSuperScript:
    case rtfSubScrShrink:
    case rtfSuperScrShrink:
    case rtfNoSuperSub:
      fmt.dwMask = CFM_SUBSCRIPT|CFM_SUPERSCRIPT;
      if (info->rtfMinor == rtfSubScrShrink) fmt.dwEffects = CFE_SUBSCRIPT;
      if (info->rtfMinor == rtfSuperScrShrink) fmt.dwEffects = CFE_SUPERSCRIPT;
      if (info->rtfMinor == rtfNoSuperSub) fmt.dwEffects = 0;
      break;
    case rtfInvisible:
      fmt.dwMask = CFM_HIDDEN;
      fmt.dwEffects = info->rtfParam ? fmt.dwMask : 0;
      break;
    case rtfBackColor:
      fmt.dwMask = CFM_BACKCOLOR;
      fmt.dwEffects = 0;
      if (info->rtfParam == 0)
        fmt.dwEffects = CFE_AUTOBACKCOLOR;
      else if (info->rtfParam != rtfNoParam)
      {
        RTFColor *c = RTFGetColor(info, info->rtfParam);
        if (c && c->rtfCBlue >= 0)
          fmt.crBackColor = (c->rtfCBlue<<16)|(c->rtfCGreen<<8)|(c->rtfCRed);
        else
          fmt.dwEffects = CFE_AUTOBACKCOLOR;
      }
      break;
    case rtfForeColor:
      fmt.dwMask = CFM_COLOR;
      fmt.dwEffects = 0;
      if (info->rtfParam == 0)
        fmt.dwEffects = CFE_AUTOCOLOR;
      else if (info->rtfParam != rtfNoParam)
      {
        RTFColor *c = RTFGetColor(info, info->rtfParam);
        if (c && c->rtfCBlue >= 0)
          fmt.crTextColor = (c->rtfCBlue<<16)|(c->rtfCGreen<<8)|(c->rtfCRed);
        else {
          fmt.dwEffects = CFE_AUTOCOLOR;
        }
      }
      break;
    case rtfFontNum:
      if (info->rtfParam != rtfNoParam)
      {
        RTFFont *f = RTFGetFont(info, info->rtfParam);
        if (f)
        {
          MultiByteToWideChar(CP_ACP, 0, f->rtfFName, -1, fmt.szFaceName, ARRAY_SIZE(fmt.szFaceName));
          fmt.szFaceName[ARRAY_SIZE(fmt.szFaceName)-1] = '\0';
          fmt.bCharSet = f->rtfFCharSet;
          fmt.dwMask = CFM_FACE | CFM_CHARSET;
          fmt.bPitchAndFamily = f->rtfFPitch | (f->rtfFFamily << 4);
        }
      }
      break;
    case rtfFontSize:
      fmt.dwMask = CFM_SIZE;
      if (info->rtfParam != rtfNoParam)
        fmt.yHeight = info->rtfParam*10;
      break;
  }
  if (fmt.dwMask) {
    ME_Style *style2;
    RTFFlushOutputBuffer(info);
    /* FIXME too slow ? how come ? */
    style2 = ME_ApplyStyle(info->editor, info->style, &fmt);
    ME_ReleaseStyle(info->style);
    info->style = style2;
    info->styleChanged = TRUE;
  }
}

/* FIXME this function doesn't get any information about context of the RTF tag, which is very bad,
   the same tags mean different things in different contexts */
void ME_RTFParAttrHook(RTF_Info *info)
{
  switch(info->rtfMinor)
  {
  case rtfParDef: /* restores default paragraph attributes */
    if (!info->editor->bEmulateVersion10) /* v4.1 */
      info->borderType = RTFBorderParaLeft;
    else /* v1.0 - 3.0 */
      info->borderType = RTFBorderParaTop;
    info->fmt.dwMask = PFM_ALIGNMENT | PFM_BORDER | PFM_LINESPACING | PFM_TABSTOPS |
        PFM_OFFSET | PFM_RIGHTINDENT | PFM_SPACEAFTER | PFM_SPACEBEFORE |
        PFM_STARTINDENT | PFM_RTLPARA | PFM_NUMBERING | PFM_NUMBERINGSTART |
        PFM_NUMBERINGSTYLE | PFM_NUMBERINGTAB;
    /* TODO: shading */
    info->fmt.wAlignment = PFA_LEFT;
    info->fmt.cTabCount = 0;
    info->fmt.dxOffset = info->fmt.dxStartIndent = info->fmt.dxRightIndent = 0;
    info->fmt.wBorderWidth = info->fmt.wBorders = 0;
    info->fmt.wBorderSpace = 0;
    info->fmt.bLineSpacingRule = 0;
    info->fmt.dySpaceBefore = info->fmt.dySpaceAfter = 0;
    info->fmt.dyLineSpacing = 0;
    info->fmt.wEffects &= ~PFE_RTLPARA;
    info->fmt.wNumbering = 0;
    info->fmt.wNumberingStart = 0;
    info->fmt.wNumberingStyle = 0;
    info->fmt.wNumberingTab = 0;

    if (!info->editor->bEmulateVersion10) /* v4.1 */
    {
      if (info->tableDef && info->tableDef->row_start &&
          info->tableDef->row_start->nFlags & MEPF_ROWEND)
      {
        ME_Cursor cursor;
        ME_Paragraph *para;
        /* We are just after a table row. */
        RTFFlushOutputBuffer(info);
        cursor = info->editor->pCursors[0];
        para = cursor.para;
        if (para == para_next( info->tableDef->row_start )
            && !cursor.nOffset && !cursor.run->nCharOfs)
        {
          /* Since the table row end, no text has been inserted, and the \intbl
           * control word has not be used.  We can confirm that we are not in a
           * table anymore.
           */
          info->tableDef->row_start = NULL;
          info->canInheritInTbl = FALSE;
        }
      }
    }
    else /* v1.0 - v3.0 */
    {
      info->fmt.dwMask |= PFM_TABLE;
      info->fmt.wEffects &= ~PFE_TABLE;
    }
    break;
  case rtfNestLevel:
    if (!info->editor->bEmulateVersion10) /* v4.1 */
    {
      while (info->rtfParam > info->nestingLevel)
      {
        RTFTable *tableDef = heap_alloc_zero(sizeof(*tableDef));
        tableDef->parent = info->tableDef;
        info->tableDef = tableDef;

        RTFFlushOutputBuffer(info);
        if (tableDef->row_start && tableDef->row_start->nFlags & MEPF_ROWEND)
        {
          ME_Paragraph *para = para_next( tableDef->row_start );
          tableDef->row_start = table_insert_row_start_at_para( info->editor, para );
        }
        else
        {
          ME_Cursor cursor;
          cursor = info->editor->pCursors[0];
          if (cursor.nOffset || cursor.run->nCharOfs)
            ME_InsertTextFromCursor(info->editor, 0, L"\r", 1, info->style);
          tableDef->row_start = table_insert_row_start( info->editor, info->editor->pCursors );
        }

        info->nestingLevel++;
      }
      info->canInheritInTbl = FALSE;
    }
    break;
  case rtfInTable:
  {
    if (!info->editor->bEmulateVersion10) /* v4.1 */
    {
      if (info->nestingLevel < 1)
      {
        RTFTable *tableDef;
        ME_Paragraph *para;

        if (!info->tableDef)
            info->tableDef = heap_alloc_zero(sizeof(*info->tableDef));
        tableDef = info->tableDef;
        RTFFlushOutputBuffer(info);
        if (tableDef->row_start && tableDef->row_start->nFlags & MEPF_ROWEND)
          para = para_next( tableDef->row_start );
        else
          para = info->editor->pCursors[0].para;

        tableDef->row_start = table_insert_row_start_at_para( info->editor, para );

        info->nestingLevel = 1;
        info->canInheritInTbl = TRUE;
      }
      return;
    } else { /* v1.0 - v3.0 */
      info->fmt.dwMask |= PFM_TABLE;
      info->fmt.wEffects |= PFE_TABLE;
    }
    break;
  }
  case rtfFirstIndent:
  case rtfLeftIndent:
    if ((info->fmt.dwMask & (PFM_STARTINDENT | PFM_OFFSET)) != (PFM_STARTINDENT | PFM_OFFSET))
    {
      PARAFORMAT2 fmt;
      fmt.cbSize = sizeof(fmt);
      editor_get_selection_para_fmt( info->editor, &fmt );
      info->fmt.dwMask |= PFM_STARTINDENT | PFM_OFFSET;
      info->fmt.dxStartIndent = fmt.dxStartIndent;
      info->fmt.dxOffset = fmt.dxOffset;
    }
    if (info->rtfMinor == rtfFirstIndent)
    {
      info->fmt.dxStartIndent += info->fmt.dxOffset + info->rtfParam;
      info->fmt.dxOffset = -info->rtfParam;
    }
    else
      info->fmt.dxStartIndent = info->rtfParam - info->fmt.dxOffset;
    break;
  case rtfRightIndent:
    info->fmt.dwMask |= PFM_RIGHTINDENT;
    info->fmt.dxRightIndent = info->rtfParam;
    break;
  case rtfQuadLeft:
  case rtfQuadJust:
    info->fmt.dwMask |= PFM_ALIGNMENT;
    info->fmt.wAlignment = PFA_LEFT;
    break;
  case rtfQuadRight:
    info->fmt.dwMask |= PFM_ALIGNMENT;
    info->fmt.wAlignment = PFA_RIGHT;
    break;
  case rtfQuadCenter:
    info->fmt.dwMask |= PFM_ALIGNMENT;
    info->fmt.wAlignment = PFA_CENTER;
    break;
  case rtfTabPos:
    if (!(info->fmt.dwMask & PFM_TABSTOPS))
    {
      PARAFORMAT2 fmt;
      fmt.cbSize = sizeof(fmt);
      editor_get_selection_para_fmt( info->editor, &fmt );
      memcpy(info->fmt.rgxTabs, fmt.rgxTabs,
             fmt.cTabCount * sizeof(fmt.rgxTabs[0]));
      info->fmt.cTabCount = fmt.cTabCount;
      info->fmt.dwMask |= PFM_TABSTOPS;
    }
    if (info->fmt.cTabCount < MAX_TAB_STOPS && info->rtfParam < 0x1000000)
      info->fmt.rgxTabs[info->fmt.cTabCount++] = info->rtfParam;
    break;
  case rtfKeep:
    info->fmt.dwMask |= PFM_KEEP;
    info->fmt.wEffects |= PFE_KEEP;
    break;
  case rtfNoWidowControl:
    info->fmt.dwMask |= PFM_NOWIDOWCONTROL;
    info->fmt.wEffects |= PFE_NOWIDOWCONTROL;
    break;
  case rtfKeepNext:
    info->fmt.dwMask |= PFM_KEEPNEXT;
    info->fmt.wEffects |= PFE_KEEPNEXT;
    break;
  case rtfSpaceAfter:
    info->fmt.dwMask |= PFM_SPACEAFTER;
    info->fmt.dySpaceAfter = info->rtfParam;
    break;
  case rtfSpaceBefore:
    info->fmt.dwMask |= PFM_SPACEBEFORE;
    info->fmt.dySpaceBefore = info->rtfParam;
    break;
  case rtfSpaceBetween:
    info->fmt.dwMask |= PFM_LINESPACING;
    if ((int)info->rtfParam > 0)
    {
      info->fmt.dyLineSpacing = info->rtfParam;
      info->fmt.bLineSpacingRule = 3;
    }
    else
    {
      info->fmt.dyLineSpacing = info->rtfParam;
      info->fmt.bLineSpacingRule = 4;
    }
    break;
  case rtfSpaceMultiply:
    info->fmt.dwMask |= PFM_LINESPACING;
    info->fmt.dyLineSpacing = info->rtfParam * 20;
    info->fmt.bLineSpacingRule = 5;
    break;
  case rtfParBullet:
    info->fmt.dwMask |= PFM_NUMBERING;
    info->fmt.wNumbering = PFN_BULLET;
    break;
  case rtfParSimple:
    info->fmt.dwMask |= PFM_NUMBERING;
    info->fmt.wNumbering = 2; /* FIXME: MSDN says it's not used ?? */
    break;
  case rtfBorderLeft:
    info->borderType = RTFBorderParaLeft;
    info->fmt.wBorders |= 1;
    info->fmt.dwMask |= PFM_BORDER;
    break;
  case rtfBorderRight:
    info->borderType = RTFBorderParaRight;
    info->fmt.wBorders |= 2;
    info->fmt.dwMask |= PFM_BORDER;
    break;
  case rtfBorderTop:
    info->borderType = RTFBorderParaTop;
    info->fmt.wBorders |= 4;
    info->fmt.dwMask |= PFM_BORDER;
    break;
  case rtfBorderBottom:
    info->borderType = RTFBorderParaBottom;
    info->fmt.wBorders |= 8;
    info->fmt.dwMask |= PFM_BORDER;
    break;
  case rtfBorderSingle:
    info->fmt.wBorders &= ~0x700;
    info->fmt.wBorders |= 1 << 8;
    info->fmt.dwMask |= PFM_BORDER;
    break;
  case rtfBorderThick:
    info->fmt.wBorders &= ~0x700;
    info->fmt.wBorders |= 2 << 8;
    info->fmt.dwMask |= PFM_BORDER;
    break;
  case rtfBorderShadow:
    info->fmt.wBorders &= ~0x700;
    info->fmt.wBorders |= 10 << 8;
    info->fmt.dwMask |= PFM_BORDER;
    break;
  case rtfBorderDouble:
    info->fmt.wBorders &= ~0x700;
    info->fmt.wBorders |= 7 << 8;
    info->fmt.dwMask |= PFM_BORDER;
    break;
  case rtfBorderDot:
    info->fmt.wBorders &= ~0x700;
    info->fmt.wBorders |= 11 << 8;
    info->fmt.dwMask |= PFM_BORDER;
    break;
  case rtfBorderWidth:
  {
    int borderSide = info->borderType & RTFBorderSideMask;
    RTFTable *tableDef = info->tableDef;
    if ((info->borderType & RTFBorderTypeMask) == RTFBorderTypeCell)
    {
      RTFBorder *border;
      if (!tableDef || tableDef->numCellsDefined >= MAX_TABLE_CELLS)
        break;
      border = &tableDef->cells[tableDef->numCellsDefined].border[borderSide];
      border->width = info->rtfParam;
      break;
    }
    info->fmt.wBorderWidth = info->rtfParam;
    info->fmt.dwMask |= PFM_BORDER;
    break;
  }
  case rtfBorderSpace:
    info->fmt.wBorderSpace = info->rtfParam;
    info->fmt.dwMask |= PFM_BORDER;
    break;
  case rtfBorderColor:
  {
    RTFTable *tableDef = info->tableDef;
    int borderSide = info->borderType & RTFBorderSideMask;
    int borderType = info->borderType & RTFBorderTypeMask;
    switch(borderType)
    {
    case RTFBorderTypePara:
      if (!info->editor->bEmulateVersion10) /* v4.1 */
        break;
      /* v1.0 - 3.0 treat paragraph and row borders the same. */
    case RTFBorderTypeRow:
      if (tableDef) {
        tableDef->border[borderSide].color = info->rtfParam;
      }
      break;
    case RTFBorderTypeCell:
      if (tableDef && tableDef->numCellsDefined < MAX_TABLE_CELLS) {
        tableDef->cells[tableDef->numCellsDefined].border[borderSide].color = info->rtfParam;
      }
      break;
    }
    break;
  }
  case rtfRTLPar:
    info->fmt.dwMask |= PFM_RTLPARA;
    info->fmt.wEffects |= PFE_RTLPARA;
    break;
  case rtfLTRPar:
    info->fmt.dwMask |= PFM_RTLPARA;
    info->fmt.wEffects &= ~PFE_RTLPARA;
    break;
  }
}

void ME_RTFTblAttrHook(RTF_Info *info)
{
  switch (info->rtfMinor)
  {
    case rtfRowDef:
    {
      if (!info->editor->bEmulateVersion10) /* v4.1 */
        info->borderType = 0; /* Not sure */
      else /* v1.0 - 3.0 */
        info->borderType = RTFBorderRowTop;
      if (!info->tableDef) {
        info->tableDef = ME_MakeTableDef(info->editor);
      } else {
        ME_InitTableDef(info->editor, info->tableDef);
      }
      break;
    }
    case rtfCellPos:
    {
      int cellNum;
      if (!info->tableDef)
      {
        info->tableDef = ME_MakeTableDef(info->editor);
      }
      cellNum = info->tableDef->numCellsDefined;
      if (cellNum >= MAX_TABLE_CELLS)
        break;
      info->tableDef->cells[cellNum].rightBoundary = info->rtfParam;
      if (cellNum < MAX_TAB_STOPS)
      {
        /* Tab stops were used to store cell positions before v4.1 but v4.1
         * still seems to set the tabstops without using them. */
        PARAFORMAT2 *fmt = &info->editor->pCursors[0].para->fmt;
        fmt->rgxTabs[cellNum] &= ~0x00FFFFFF;
        fmt->rgxTabs[cellNum] |= 0x00FFFFFF & info->rtfParam;
      }
      info->tableDef->numCellsDefined++;
      break;
    }
    case rtfRowBordTop:
      info->borderType = RTFBorderRowTop;
      break;
    case rtfRowBordLeft:
      info->borderType = RTFBorderRowLeft;
      break;
    case rtfRowBordBottom:
      info->borderType = RTFBorderRowBottom;
      break;
    case rtfRowBordRight:
      info->borderType = RTFBorderRowRight;
      break;
    case rtfCellBordTop:
      info->borderType = RTFBorderCellTop;
      break;
    case rtfCellBordLeft:
      info->borderType = RTFBorderCellLeft;
      break;
    case rtfCellBordBottom:
      info->borderType = RTFBorderCellBottom;
      break;
    case rtfCellBordRight:
      info->borderType = RTFBorderCellRight;
      break;
    case rtfRowGapH:
      if (info->tableDef)
        info->tableDef->gapH = info->rtfParam;
      break;
    case rtfRowLeftEdge:
      if (info->tableDef)
        info->tableDef->leftEdge = info->rtfParam;
      break;
  }
}

void ME_RTFSpecialCharHook(RTF_Info *info)
{
  RTFTable *tableDef = info->tableDef;
  switch (info->rtfMinor)
  {
    case rtfNestCell:
      if (info->editor->bEmulateVersion10) /* v1.0 - v3.0 */
        break;
      /* else fall through since v4.1 treats rtfNestCell and rtfCell the same */
    case rtfCell:
      if (!tableDef)
        break;
      RTFFlushOutputBuffer(info);
      if (!info->editor->bEmulateVersion10) /* v4.1 */
      {
        if (tableDef->row_start)
        {
          if (!info->nestingLevel && tableDef->row_start->nFlags & MEPF_ROWEND)
          {
            ME_Paragraph *para = para_next( tableDef->row_start );
            tableDef->row_start = table_insert_row_start_at_para( info->editor, para );
            info->nestingLevel = 1;
          }
          table_insert_cell( info->editor, info->editor->pCursors );
        }
      }
      else /* v1.0 - v3.0 */
      {
        ME_Paragraph *para = info->editor->pCursors[0].para;

        if (para_in_table( para ) && tableDef->numCellsInserted < tableDef->numCellsDefined)
        {
          WCHAR tab = '\t';
          ME_InsertTextFromCursor(info->editor, 0, &tab, 1, info->style);
          tableDef->numCellsInserted++;
        }
      }
      break;
    case rtfNestRow:
      if (info->editor->bEmulateVersion10) /* v1.0 - v3.0 */
        break;
      /* else fall through since v4.1 treats rtfNestRow and rtfRow the same */
    case rtfRow:
    {
      ME_Run *run;
      ME_Paragraph *para;
      ME_Cell *cell;
      int i;

      if (!tableDef)
        break;
      RTFFlushOutputBuffer(info);
      if (!info->editor->bEmulateVersion10) /* v4.1 */
      {
        if (!tableDef->row_start) break;
        if (!info->nestingLevel && tableDef->row_start->nFlags & MEPF_ROWEND)
        {
          para = para_next( tableDef->row_start );
          tableDef->row_start = table_insert_row_start_at_para( info->editor, para );
          info->nestingLevel++;
        }
        para = tableDef->row_start;
        cell = table_row_first_cell( para );
        assert( cell && !cell_prev( cell ) );
        if (tableDef->numCellsDefined < 1)
        {
          /* 2000 twips appears to be the cell size that native richedit uses
           * when no cell sizes are specified. */
          const int default_size = 2000;
          int right_boundary = default_size;
          cell->nRightBoundary = right_boundary;
          while (cell_next( cell ))
          {
            cell = cell_next( cell );
            right_boundary += default_size;
            cell->nRightBoundary = right_boundary;
          }
          para = table_insert_cell( info->editor, info->editor->pCursors );
          cell = para_cell( para );
          cell->nRightBoundary = right_boundary;
        }
        else
        {
          for (i = 0; i < tableDef->numCellsDefined; i++)
          {
            RTFCell *cellDef = &tableDef->cells[i];
            cell->nRightBoundary = cellDef->rightBoundary;
            ME_ApplyBorderProperties( info, &cell->border, cellDef->border );
            cell = cell_next( cell );
            if (!cell)
            {
              para = table_insert_cell( info->editor, info->editor->pCursors );
              cell = para_cell( para );
            }
          }
          /* Cell for table row delimiter is empty */
          cell->nRightBoundary = tableDef->cells[i - 1].rightBoundary;
        }

        run = para_first_run( cell_first_para( cell ) );
        if (info->editor->pCursors[0].run != run || info->editor->pCursors[0].nOffset)
        {
          int nOfs, nChars;
          /* Delete inserted cells that aren't defined. */
          info->editor->pCursors[1].run = run;
          info->editor->pCursors[1].para = run->para;
          info->editor->pCursors[1].nOffset = 0;
          nOfs = ME_GetCursorOfs(&info->editor->pCursors[1]);
          nChars = ME_GetCursorOfs(&info->editor->pCursors[0]) - nOfs;
          ME_InternalDeleteText(info->editor, &info->editor->pCursors[1],
                                nChars, TRUE);
        }

        para = table_insert_row_end( info->editor, info->editor->pCursors );
        para->fmt.dxOffset = abs(info->tableDef->gapH);
        para->fmt.dxStartIndent = info->tableDef->leftEdge;
        ME_ApplyBorderProperties( info, &para->border, tableDef->border );
        info->nestingLevel--;
        if (!info->nestingLevel)
        {
          if (info->canInheritInTbl) tableDef->row_start = para;
          else
          {
            while (info->tableDef)
            {
              tableDef = info->tableDef;
              info->tableDef = tableDef->parent;
              heap_free(tableDef);
            }
          }
        }
        else
        {
          info->tableDef = tableDef->parent;
          heap_free(tableDef);
        }
      }
      else /* v1.0 - v3.0 */
      {
        para = info->editor->pCursors[0].para;
        para->fmt.dxOffset = info->tableDef->gapH;
        para->fmt.dxStartIndent = info->tableDef->leftEdge;

        ME_ApplyBorderProperties( info, &para->border, tableDef->border );
        while (tableDef->numCellsInserted < tableDef->numCellsDefined)
        {
          WCHAR tab = '\t';
          ME_InsertTextFromCursor(info->editor, 0, &tab, 1, info->style);
          tableDef->numCellsInserted++;
        }
        para->fmt.cTabCount = min(tableDef->numCellsDefined, MAX_TAB_STOPS);
        if (!tableDef->numCellsDefined) para->fmt.wEffects &= ~PFE_TABLE;
        ME_InsertTextFromCursor(info->editor, 0, L"\r", 1, info->style);
        tableDef->numCellsInserted = 0;
      }
      break;
    }
    case rtfTab:
    case rtfPar:
      if (info->editor->bEmulateVersion10) /* v1.0 - 3.0 */
      {
        ME_Paragraph *para;

        RTFFlushOutputBuffer(info);
        para = info->editor->pCursors[0].para;
        if (para_in_table( para ))
        {
          /* rtfPar is treated like a space within a table. */
          info->rtfClass = rtfText;
          info->rtfMajor = ' ';
        }
        else if (info->rtfMinor == rtfPar && tableDef)
          tableDef->numCellsInserted = 0;
      }
      break;
  }
}

static HRESULT insert_static_object(ME_TextEditor *editor, HENHMETAFILE hemf, HBITMAP hbmp,
                                    const SIZEL* sz)
{
  LPOLEOBJECT         lpObject = NULL;
  LPSTORAGE           lpStorage = NULL;
  LPOLECLIENTSITE     lpClientSite = NULL;
  LPDATAOBJECT        lpDataObject = NULL;
  LPOLECACHE          lpOleCache = NULL;
  LPRICHEDITOLE       lpReOle = NULL;
  STGMEDIUM           stgm;
  FORMATETC           fm;
  CLSID               clsid;
  HRESULT             hr = E_FAIL;
  DWORD               conn;

  if (hemf)
  {
      stgm.tymed = TYMED_ENHMF;
      stgm.u.hEnhMetaFile = hemf;
      fm.cfFormat = CF_ENHMETAFILE;
  }
  else if (hbmp)
  {
      stgm.tymed = TYMED_GDI;
      stgm.u.hBitmap = hbmp;
      fm.cfFormat = CF_BITMAP;
  }
  else return E_FAIL;

  stgm.pUnkForRelease = NULL;

  fm.ptd = NULL;
  fm.dwAspect = DVASPECT_CONTENT;
  fm.lindex = -1;
  fm.tymed = stgm.tymed;

  if (!editor->reOle)
  {
    if (!CreateIRichEditOle(NULL, editor, (LPVOID *)&editor->reOle))
      return hr;
  }

  if (OleCreateDefaultHandler(&CLSID_NULL, NULL, &IID_IOleObject, (void**)&lpObject) == S_OK &&
      IUnknown_QueryInterface(editor->reOle, &IID_IRichEditOle, (void**)&lpReOle) == S_OK &&
      IRichEditOle_GetClientSite(lpReOle, &lpClientSite) == S_OK &&
      IOleObject_SetClientSite(lpObject, lpClientSite) == S_OK &&
      IOleObject_GetUserClassID(lpObject, &clsid) == S_OK &&
      IOleObject_QueryInterface(lpObject, &IID_IOleCache, (void**)&lpOleCache) == S_OK &&
      IOleCache_Cache(lpOleCache, &fm, 0, &conn) == S_OK &&
      IOleObject_QueryInterface(lpObject, &IID_IDataObject, (void**)&lpDataObject) == S_OK &&
      IDataObject_SetData(lpDataObject, &fm, &stgm, TRUE) == S_OK)
  {
    REOBJECT            reobject;

    reobject.cbStruct = sizeof(reobject);
    reobject.cp = REO_CP_SELECTION;
    reobject.clsid = clsid;
    reobject.poleobj = lpObject;
    reobject.pstg = lpStorage;
    reobject.polesite = lpClientSite;
    /* convert from twips to .01 mm */
    reobject.sizel.cx = MulDiv(sz->cx, 254, 144);
    reobject.sizel.cy = MulDiv(sz->cy, 254, 144);
    reobject.dvaspect = DVASPECT_CONTENT;
    reobject.dwFlags = 0; /* FIXME */
    reobject.dwUser = 0;

    ME_InsertOLEFromCursor(editor, &reobject, 0);
    hr = S_OK;
  }

  if (lpObject)       IOleObject_Release(lpObject);
  if (lpClientSite)   IOleClientSite_Release(lpClientSite);
  if (lpStorage)      IStorage_Release(lpStorage);
  if (lpDataObject)   IDataObject_Release(lpDataObject);
  if (lpOleCache)     IOleCache_Release(lpOleCache);
  if (lpReOle)        IRichEditOle_Release(lpReOle);

  return hr;
}

static void ME_RTFReadShpPictGroup( RTF_Info *info )
{
    int level = 1;

    for (;;)
    {
        RTFGetToken (info);

        if (info->rtfClass == rtfEOF) return;
        if (RTFCheckCM( info, rtfGroup, rtfEndGroup ))
        {
            if (--level == 0) break;
        }
        else if (RTFCheckCM( info, rtfGroup, rtfBeginGroup ))
        {
            level++;
        }
        else
        {
            RTFRouteToken( info );
            if (RTFCheckCM( info, rtfGroup, rtfEndGroup ))
                level--;
        }
    }

    RTFRouteToken( info ); /* feed "}" back to router */
    return;
}

static DWORD read_hex_data( RTF_Info *info, BYTE **out )
{
    DWORD read = 0, size = 1024;
    BYTE *buf, val;
    BOOL flip;

    *out = NULL;

    if (info->rtfClass != rtfText)
    {
        ERR("Called with incorrect token\n");
        return 0;
    }

    buf = HeapAlloc( GetProcessHeap(), 0, size );
    if (!buf) return 0;

    val = info->rtfMajor;
    for (flip = TRUE;; flip = !flip)
    {
        RTFGetToken( info );
        if (info->rtfClass == rtfEOF)
        {
            HeapFree( GetProcessHeap(), 0, buf );
            return 0;
        }
        if (info->rtfClass != rtfText) break;
        if (flip)
        {
            if (read >= size)
            {
                size *= 2;
                buf = HeapReAlloc( GetProcessHeap(), 0, buf, size );
                if (!buf) return 0;
            }
            buf[read++] = RTFCharToHex(val) * 16 + RTFCharToHex(info->rtfMajor);
        }
        else
            val = info->rtfMajor;
    }
    if (flip) FIXME("wrong hex string\n");

    *out = buf;
    return read;
}

static void ME_RTFReadPictGroup(RTF_Info *info)
{
    SIZEL sz;
    BYTE *buffer = NULL;
    DWORD size = 0;
    METAFILEPICT mfp;
    HENHMETAFILE hemf;
    HBITMAP hbmp;
    enum gfxkind {gfx_unknown = 0, gfx_enhmetafile, gfx_metafile, gfx_dib} gfx = gfx_unknown;
    int level = 1;

    mfp.mm = MM_TEXT;
    sz.cx = sz.cy = 0;

    for (;;)
    {
        RTFGetToken( info );

        if (info->rtfClass == rtfText)
        {
            if (level == 1)
            {
                if (!buffer)
                    size = read_hex_data( info, &buffer );
            }
            else
            {
                RTFSkipGroup( info );
            }
        } /* We potentially have a new token so fall through. */

        if (info->rtfClass == rtfEOF) return;

        if (RTFCheckCM( info, rtfGroup, rtfEndGroup ))
        {
            if (--level == 0) break;
            continue;
        }
        if (RTFCheckCM( info, rtfGroup, rtfBeginGroup ))
        {
            level++;
            continue;
        }
        if (!RTFCheckCM( info, rtfControl, rtfPictAttr ))
        {
            RTFRouteToken( info );
            if (RTFCheckCM( info, rtfGroup, rtfEndGroup ))
                level--;
            continue;
        }

        if (RTFCheckMM( info, rtfPictAttr, rtfWinMetafile ))
        {
            mfp.mm = info->rtfParam;
            gfx = gfx_metafile;
        }
        else if (RTFCheckMM( info, rtfPictAttr, rtfDevIndBitmap ))
        {
            if (info->rtfParam != 0) FIXME("dibitmap should be 0 (%d)\n", info->rtfParam);
            gfx = gfx_dib;
        }
        else if (RTFCheckMM( info, rtfPictAttr, rtfEmfBlip ))
            gfx = gfx_enhmetafile;
        else if (RTFCheckMM( info, rtfPictAttr, rtfPicWid ))
            mfp.xExt = info->rtfParam;
        else if (RTFCheckMM( info, rtfPictAttr, rtfPicHt ))
            mfp.yExt = info->rtfParam;
        else if (RTFCheckMM( info, rtfPictAttr, rtfPicGoalWid ))
            sz.cx = info->rtfParam;
        else if (RTFCheckMM( info, rtfPictAttr, rtfPicGoalHt ))
            sz.cy = info->rtfParam;
        else
            FIXME("Non supported attribute: %d %d %d\n", info->rtfClass, info->rtfMajor, info->rtfMinor);
    }

    if (buffer)
    {
        switch (gfx)
        {
        case gfx_enhmetafile:
            if ((hemf = SetEnhMetaFileBits( size, buffer )))
                insert_static_object( info->editor, hemf, NULL, &sz );
            break;
        case gfx_metafile:
            if ((hemf = SetWinMetaFileBits( size, buffer, NULL, &mfp )))
                insert_static_object( info->editor, hemf, NULL, &sz );
            break;
        case gfx_dib:
        {
            BITMAPINFO *bi = (BITMAPINFO*)buffer;
            HDC hdc = GetDC(0);
            unsigned nc = bi->bmiHeader.biClrUsed;

            /* not quite right, especially for bitfields type of compression */
            if (!nc && bi->bmiHeader.biBitCount <= 8)
                nc = 1 << bi->bmiHeader.biBitCount;
            if ((hbmp = CreateDIBitmap( hdc, &bi->bmiHeader,
                                        CBM_INIT, (char*)(bi + 1) + nc * sizeof(RGBQUAD),
                                        bi, DIB_RGB_COLORS)) )
                insert_static_object( info->editor, NULL, hbmp, &sz );
            ReleaseDC( 0, hdc );
            break;
        }
        default:
            break;
        }
    }
    HeapFree( GetProcessHeap(), 0, buffer );
    RTFRouteToken( info ); /* feed "}" back to router */
    return;
}

/* for now, lookup the \result part and use it, whatever the object */
static void ME_RTFReadObjectGroup(RTF_Info *info)
{
  for (;;)
  {
    RTFGetToken (info);
    if (info->rtfClass == rtfEOF)
      return;
    if (RTFCheckCM(info, rtfGroup, rtfEndGroup))
      break;
    if (RTFCheckCM(info, rtfGroup, rtfBeginGroup))
    {
      RTFGetToken (info);
      if (info->rtfClass == rtfEOF)
        return;
      if (RTFCheckCMM(info, rtfControl, rtfDestination, rtfObjResult))
      {
	int	level = 1;

	while (RTFGetToken (info) != rtfEOF)
	{
          if (info->rtfClass == rtfGroup)
          {
            if (info->rtfMajor == rtfBeginGroup) level++;
            else if (info->rtfMajor == rtfEndGroup && --level < 0) break;
          }
          RTFRouteToken(info);
	}
      }
      else RTFSkipGroup(info);
      continue;
    }
    if (!RTFCheckCM (info, rtfControl, rtfObjAttr))
    {
      FIXME("Non supported attribute: %d %d %d\n", info->rtfClass, info->rtfMajor, info->rtfMinor);
      return;
    }
  }
  RTFRouteToken(info);	/* feed "}" back to router */
}

static void ME_RTFReadParnumGroup( RTF_Info *info )
{
    int level = 1, type = -1;
    WORD indent = 0, start = 1;
    WCHAR txt_before = 0, txt_after = 0;

    for (;;)
    {
        RTFGetToken( info );

        if (RTFCheckCMM( info, rtfControl, rtfDestination, rtfParNumTextBefore ) ||
            RTFCheckCMM( info, rtfControl, rtfDestination, rtfParNumTextAfter ))
        {
            int loc = info->rtfMinor;

            RTFGetToken( info );
            if (info->rtfClass == rtfText)
            {
                if (loc == rtfParNumTextBefore)
                    txt_before = info->rtfMajor;
                else
                    txt_after = info->rtfMajor;
                continue;
            }
            /* falling through to catch EOFs and group level changes */
        }

        if (info->rtfClass == rtfEOF)
            return;

        if (RTFCheckCM( info, rtfGroup, rtfEndGroup ))
        {
            if (--level == 0) break;
            continue;
        }

        if (RTFCheckCM( info, rtfGroup, rtfBeginGroup ))
        {
            level++;
            continue;
        }

        /* Ignore non para-attr */
        if (!RTFCheckCM( info, rtfControl, rtfParAttr ))
            continue;

        switch (info->rtfMinor)
        {
        case rtfParLevel: /* Para level is ignored */
        case rtfParSimple:
            break;
        case rtfParBullet:
            type = PFN_BULLET;
            break;

        case rtfParNumDecimal:
            type = PFN_ARABIC;
            break;
        case rtfParNumULetter:
            type = PFN_UCLETTER;
            break;
        case rtfParNumURoman:
            type = PFN_UCROMAN;
            break;
        case rtfParNumLLetter:
            type = PFN_LCLETTER;
            break;
        case rtfParNumLRoman:
            type = PFN_LCROMAN;
            break;

        case rtfParNumIndent:
            indent = info->rtfParam;
            break;
        case rtfParNumStartAt:
            start = info->rtfParam;
            break;
        }
    }

    if (type != -1)
    {
        info->fmt.dwMask |= (PFM_NUMBERING | PFM_NUMBERINGSTART | PFM_NUMBERINGSTYLE | PFM_NUMBERINGTAB);
        info->fmt.wNumbering = type;
        info->fmt.wNumberingStart = start;
        info->fmt.wNumberingStyle = PFNS_PAREN;
        if (type != PFN_BULLET)
        {
            if (txt_before == 0 && txt_after == 0)
                info->fmt.wNumberingStyle = PFNS_PLAIN;
            else if (txt_after == '.')
                info->fmt.wNumberingStyle = PFNS_PERIOD;
            else if (txt_before == '(' && txt_after == ')')
                info->fmt.wNumberingStyle = PFNS_PARENS;
        }
        info->fmt.wNumberingTab = indent;
    }

    TRACE("type %d indent %d start %d txt before %04x txt after %04x\n",
          type, indent, start, txt_before, txt_after);

    RTFRouteToken( info );     /* feed "}" back to router */
}

static void ME_RTFReadHook(RTF_Info *info)
{
  switch(info->rtfClass)
  {
    case rtfGroup:
      switch(info->rtfMajor)
      {
        case rtfBeginGroup:
          if (info->stackTop < maxStack) {
            info->stack[info->stackTop].style = info->style;
            ME_AddRefStyle(info->style);
            info->stack[info->stackTop].codePage = info->codePage;
            info->stack[info->stackTop].unicodeLength = info->unicodeLength;
          }
          info->stackTop++;
          info->styleChanged = FALSE;
          break;
        case rtfEndGroup:
        {
          RTFFlushOutputBuffer(info);
          info->stackTop--;
          if (info->stackTop <= 0)
            info->rtfClass = rtfEOF;
          if (info->stackTop < 0)
            return;

          ME_ReleaseStyle(info->style);
          info->style = info->stack[info->stackTop].style;
          info->codePage = info->stack[info->stackTop].codePage;
          info->unicodeLength = info->stack[info->stackTop].unicodeLength;
          break;
        }
      }
      break;
  }
}

void
ME_StreamInFill(ME_InStream *stream)
{
  stream->editstream->dwError = stream->editstream->pfnCallback(stream->editstream->dwCookie,
                                                                (BYTE *)stream->buffer,
                                                                sizeof(stream->buffer),
                                                                (LONG *)&stream->dwSize);
  stream->dwUsed = 0;
}

static LRESULT ME_StreamIn(ME_TextEditor *editor, DWORD format, EDITSTREAM *stream, BOOL stripLastCR)
{
  RTF_Info parser;
  ME_Style *style;
  int from, to, nUndoMode;
  int nEventMask = editor->nEventMask;
  ME_InStream inStream;
  BOOL invalidRTF = FALSE;
  ME_Cursor *selStart, *selEnd;
  LRESULT num_read = 0; /* bytes read for SF_TEXT, non-control chars inserted for SF_RTF */

  TRACE("stream==%p editor==%p format==0x%X\n", stream, editor, format);
  editor->nEventMask = 0;

  ME_GetSelectionOfs(editor, &from, &to);
  if (format & SFF_SELECTION && editor->mode & TM_RICHTEXT)
  {
    ME_GetSelection(editor, &selStart, &selEnd);
    style = ME_GetSelectionInsertStyle(editor);

    ME_InternalDeleteText(editor, selStart, to - from, FALSE);

    /* Don't insert text at the end of the table row */
    if (!editor->bEmulateVersion10) /* v4.1 */
    {
      ME_Paragraph *para = editor->pCursors->para;
      if (para->nFlags & (MEPF_ROWSTART | MEPF_ROWEND))
      {
        para = para_next( para );
        editor->pCursors[0].para = para;
        editor->pCursors[0].run = para_first_run( para );
        editor->pCursors[0].nOffset = 0;
      }
      editor->pCursors[1] = editor->pCursors[0];
    }
    else /* v1.0 - 3.0 */
    {
      if (editor->pCursors[0].run->nFlags & MERF_ENDPARA &&
          para_in_table( editor->pCursors[0].para ))
        return 0;
    }
  }
  else
  {
    style = editor->pBuffer->pDefaultStyle;
    ME_AddRefStyle(style);
    set_selection_cursors(editor, 0, 0);
    ME_InternalDeleteText(editor, &editor->pCursors[1],
                          ME_GetTextLength(editor), FALSE);
    from = to = 0;
    ME_ClearTempStyle(editor);
    editor_set_default_para_fmt( editor, &editor->pCursors[0].para->fmt );
  }


  /* Back up undo mode to a local variable */
  nUndoMode = editor->nUndoMode;

  /* Only create an undo if SFF_SELECTION is set */
  if (!(format & SFF_SELECTION))
    editor->nUndoMode = umIgnore;

  inStream.editstream = stream;
  inStream.editstream->dwError = 0;
  inStream.dwSize = 0;
  inStream.dwUsed = 0;

  if (format & SF_RTF)
  {
    /* Check if it's really RTF, and if it is not, use plain text */
    ME_StreamInFill(&inStream);
    if (!inStream.editstream->dwError)
    {
      if ((!editor->bEmulateVersion10 && strncmp(inStream.buffer, "{\\rtf", 5) && strncmp(inStream.buffer, "{\\urtf", 6))
	|| (editor->bEmulateVersion10 && *inStream.buffer != '{'))
      {
        invalidRTF = TRUE;
        inStream.editstream->dwError = -16;
      }
    }
  }

  if (!invalidRTF && !inStream.editstream->dwError)
  {
    ME_Cursor start;
    from = ME_GetCursorOfs(&editor->pCursors[0]);
    if (format & SF_RTF) {

      /* setup the RTF parser */
      memset(&parser, 0, sizeof parser);
      RTFSetEditStream(&parser, &inStream);
      parser.rtfFormat = format&(SF_TEXT|SF_RTF);
      parser.editor = editor;
      parser.style = style;
      WriterInit(&parser);
      RTFInit(&parser);
      RTFSetReadHook(&parser, ME_RTFReadHook);
      RTFSetDestinationCallback(&parser, rtfShpPict, ME_RTFReadShpPictGroup);
      RTFSetDestinationCallback(&parser, rtfPict, ME_RTFReadPictGroup);
      RTFSetDestinationCallback(&parser, rtfObject, ME_RTFReadObjectGroup);
      RTFSetDestinationCallback(&parser, rtfParNumbering, ME_RTFReadParnumGroup);
      if (!parser.editor->bEmulateVersion10) /* v4.1 */
      {
        RTFSetDestinationCallback(&parser, rtfNoNestTables, RTFSkipGroup);
        RTFSetDestinationCallback(&parser, rtfNestTableProps, RTFReadGroup);
      }
      BeginFile(&parser);

      /* do the parsing */
      RTFRead(&parser);
      RTFFlushOutputBuffer(&parser);
      if (!editor->bEmulateVersion10) /* v4.1 */
      {
        if (parser.tableDef && parser.tableDef->row_start &&
            (parser.nestingLevel > 0 || parser.canInheritInTbl))
        {
          /* Delete any incomplete table row at the end of the rich text. */
          int nOfs, nChars;
          ME_Paragraph *para;

          parser.rtfMinor = rtfRow;
          /* Complete the table row before deleting it.
           * By doing it this way we will have the current paragraph format set
           * properly to reflect that is not in the complete table, and undo items
           * will be added for this change to the current paragraph format. */
          if (parser.nestingLevel > 0)
          {
            while (parser.nestingLevel > 1)
              ME_RTFSpecialCharHook(&parser); /* Decrements nestingLevel */
            para = parser.tableDef->row_start;
            ME_RTFSpecialCharHook(&parser);
          }
          else
          {
            para = parser.tableDef->row_start;
            ME_RTFSpecialCharHook(&parser);
            assert( para->nFlags & MEPF_ROWEND );
            para = para_next( para );
          }

          editor->pCursors[1].para = para;
          editor->pCursors[1].run = para_first_run( para );
          editor->pCursors[1].nOffset = 0;
          nOfs = ME_GetCursorOfs(&editor->pCursors[1]);
          nChars = ME_GetCursorOfs(&editor->pCursors[0]) - nOfs;
          ME_InternalDeleteText(editor, &editor->pCursors[1], nChars, TRUE);
          if (parser.tableDef) parser.tableDef->row_start = NULL;
        }
      }
      RTFDestroy(&parser);

      if (parser.stackTop > 0)
      {
        while (--parser.stackTop >= 0)
        {
          ME_ReleaseStyle(parser.style);
          parser.style = parser.stack[parser.stackTop].style;
        }
        if (!inStream.editstream->dwError)
          inStream.editstream->dwError = HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
      }

      /* Remove last line break, as mandated by tests. This is not affected by
         CR/LF counters, since RTF streaming presents only \para tokens, which
         are converted according to the standard rules: \r for 2.0, \r\n for 1.0
       */
      if (stripLastCR && !(format & SFF_SELECTION)) {
        int newto;
        ME_GetSelection(editor, &selStart, &selEnd);
        newto = ME_GetCursorOfs(selEnd);
        if (newto > to + (editor->bEmulateVersion10 ? 1 : 0)) {
          WCHAR lastchar[3] = {'\0', '\0'};
          int linebreakSize = editor->bEmulateVersion10 ? 2 : 1;
          ME_Cursor linebreakCursor = *selEnd, lastcharCursor = *selEnd;
          CHARFORMAT2W cf;

          /* Set the final eop to the char fmt of the last char */
          cf.cbSize = sizeof(cf);
          cf.dwMask = CFM_ALL2;
          ME_MoveCursorChars(editor, &lastcharCursor, -1, FALSE);
          ME_GetCharFormat(editor, &lastcharCursor, &linebreakCursor, &cf);
          set_selection_cursors(editor, newto, -1);
          ME_SetSelectionCharFormat(editor, &cf);
          set_selection_cursors(editor, newto, newto);

          ME_MoveCursorChars(editor, &linebreakCursor, -linebreakSize, FALSE);
          ME_GetTextW(editor, lastchar, 2, &linebreakCursor, linebreakSize, FALSE, FALSE);
          if (lastchar[0] == '\r' && (lastchar[1] == '\n' || lastchar[1] == '\0')) {
            ME_InternalDeleteText(editor, &linebreakCursor, linebreakSize, FALSE);
          }
        }
      }
      to = ME_GetCursorOfs(&editor->pCursors[0]);
      num_read = to - from;

      style = parser.style;
    }
    else if (format & SF_TEXT)
    {
      num_read = ME_StreamInText(editor, format, &inStream, style);
      to = ME_GetCursorOfs(&editor->pCursors[0]);
    }
    else
      ERR("EM_STREAMIN without SF_TEXT or SF_RTF\n");
    /* put the cursor at the top */
    if (!(format & SFF_SELECTION))
      set_selection_cursors(editor, 0, 0);
    cursor_from_char_ofs( editor, from, &start );
    ME_UpdateLinkAttribute(editor, &start, to - from);
  }

  /* Restore saved undo mode */
  editor->nUndoMode = nUndoMode;

  /* even if we didn't add an undo, we need to commit anything on the stack */
  ME_CommitUndo(editor);

  /* If SFF_SELECTION isn't set, delete any undos from before we started too */
  if (!(format & SFF_SELECTION))
    ME_EmptyUndoStack(editor);

  ME_ReleaseStyle(style);
  editor->nEventMask = nEventMask;
  ME_UpdateRepaint(editor, FALSE);
  if (!(format & SFF_SELECTION)) {
    ME_ClearTempStyle(editor);
  }
  update_caret(editor);
  ME_SendSelChange(editor);
  ME_SendRequestResize(editor, FALSE);

  return num_read;
}


typedef struct tagME_RTFStringStreamStruct
{
  char *string;
  int pos;
  int length;
} ME_RTFStringStreamStruct;

static DWORD CALLBACK ME_ReadFromRTFString(DWORD_PTR dwCookie, LPBYTE lpBuff, LONG cb, LONG *pcb)
{
  ME_RTFStringStreamStruct *pStruct = (ME_RTFStringStreamStruct *)dwCookie;
  int count;

  count = min(cb, pStruct->length - pStruct->pos);
  memmove(lpBuff, pStruct->string + pStruct->pos, count);
  pStruct->pos += count;
  *pcb = count;
  return 0;
}

static void
ME_StreamInRTFString(ME_TextEditor *editor, BOOL selection, char *string)
{
  EDITSTREAM es;
  ME_RTFStringStreamStruct data;

  data.string = string;
  data.length = strlen(string);
  data.pos = 0;
  es.dwCookie = (DWORD_PTR)&data;
  es.pfnCallback = ME_ReadFromRTFString;
  ME_StreamIn(editor, SF_RTF | (selection ? SFF_SELECTION : 0), &es, TRUE);
}


static int
ME_FindText(ME_TextEditor *editor, DWORD flags, const CHARRANGE *chrg, const WCHAR *text, CHARRANGE *chrgText)
{
  const int nLen = lstrlenW(text);
  const int nTextLen = ME_GetTextLength(editor);
  int nMin, nMax;
  ME_Cursor cursor;
  WCHAR wLastChar = ' ';

  TRACE("flags==0x%08x, chrg->cpMin==%d, chrg->cpMax==%d text==%s\n",
        flags, chrg->cpMin, chrg->cpMax, debugstr_w(text));

  if (flags & ~(FR_DOWN | FR_MATCHCASE | FR_WHOLEWORD))
    FIXME("Flags 0x%08x not implemented\n",
        flags & ~(FR_DOWN | FR_MATCHCASE | FR_WHOLEWORD));

  nMin = chrg->cpMin;
  if (chrg->cpMax == -1)
    nMax = nTextLen;
  else
    nMax = chrg->cpMax > nTextLen ? nTextLen : chrg->cpMax;

  /* In 1.0 emulation, if cpMax reaches end of text, add the FR_DOWN flag */
  if (editor->bEmulateVersion10 && nMax == nTextLen)
  {
    flags |= FR_DOWN;
  }

  /* In 1.0 emulation, cpMin must always be no greater than cpMax */
  if (editor->bEmulateVersion10 && nMax < nMin)
  {
    if (chrgText)
    {
      chrgText->cpMin = -1;
      chrgText->cpMax = -1;
    }
    return -1;
  }

  /* when searching up, if cpMin < cpMax, then instead of searching
   * on [cpMin,cpMax], we search on [0,cpMin], otherwise, search on
   * [cpMax, cpMin]. The exception is when cpMax is -1, in which
   * case, it is always bigger than cpMin.
   */
  if (!editor->bEmulateVersion10 && !(flags & FR_DOWN))
  {
    int nSwap = nMax;

    nMax = nMin > nTextLen ? nTextLen : nMin;
    if (nMin < nSwap || chrg->cpMax == -1)
      nMin = 0;
    else
      nMin = nSwap;
  }

  if (!nLen || nMin < 0 || nMax < 0 || nMax < nMin)
  {
    if (chrgText)
      chrgText->cpMin = chrgText->cpMax = -1;
    return -1;
  }

  if (flags & FR_DOWN) /* Forward search */
  {
    /* If possible, find the character before where the search starts */
    if ((flags & FR_WHOLEWORD) && nMin)
    {
      cursor_from_char_ofs( editor, nMin - 1, &cursor );
      wLastChar = *get_text( cursor.run, cursor.nOffset );
      ME_MoveCursorChars(editor, &cursor, 1, FALSE);
    }
    else cursor_from_char_ofs( editor, nMin, &cursor );

    while (cursor.run && ME_GetCursorOfs(&cursor) + nLen <= nMax)
    {
      ME_Run *run = cursor.run;
      int nCurStart = cursor.nOffset;
      int nMatched = 0;
    
      while (run && ME_CharCompare( *get_text( run, nCurStart + nMatched ), text[nMatched], (flags & FR_MATCHCASE)))
      {
        if ((flags & FR_WHOLEWORD) && iswalnum(wLastChar))
          break;

        nMatched++;
        if (nMatched == nLen)
        {
          ME_Run *next_run = run;
          int nNextStart = nCurStart;
          WCHAR wNextChar;

          /* Check to see if next character is a whitespace */
          if (flags & FR_WHOLEWORD)
          {
            if (nCurStart + nMatched == run->len)
            {
              next_run = run_next_all_paras( run );
              nNextStart = -nMatched;
            }

            if (next_run)
              wNextChar = *get_text( next_run, nNextStart + nMatched );
            else
              wNextChar = ' ';

            if (iswalnum(wNextChar))
              break;
          }

          cursor.nOffset += cursor.para->nCharOfs + cursor.run->nCharOfs;
          if (chrgText)
          {
            chrgText->cpMin = cursor.nOffset;
            chrgText->cpMax = cursor.nOffset + nLen;
          }
          TRACE("found at %d-%d\n", cursor.nOffset, cursor.nOffset + nLen);
          return cursor.nOffset;
        }
        if (nCurStart + nMatched == run->len)
        {
          run = run_next_all_paras( run );
          nCurStart = -nMatched;
        }
      }
      if (run)
        wLastChar = *get_text( run, nCurStart + nMatched );
      else
        wLastChar = ' ';

      cursor.nOffset++;
      if (cursor.nOffset == cursor.run->len)
      {
        if (run_next_all_paras( cursor.run ))
        {
          cursor.run = run_next_all_paras( cursor.run );
          cursor.para = cursor.run->para;
          cursor.nOffset = 0;
        }
        else
          cursor.run = NULL;
      }
    }
  }
  else /* Backward search */
  {
    /* If possible, find the character after where the search ends */
    if ((flags & FR_WHOLEWORD) && nMax < nTextLen - 1)
    {
      cursor_from_char_ofs( editor, nMax + 1, &cursor );
      wLastChar = *get_text( cursor.run, cursor.nOffset );
      ME_MoveCursorChars(editor, &cursor, -1, FALSE);
    }
    else cursor_from_char_ofs( editor, nMax, &cursor );

    while (cursor.run && ME_GetCursorOfs(&cursor) - nLen >= nMin)
    {
      ME_Run *run = cursor.run;
      ME_Paragraph *para = cursor.para;
      int nCurEnd = cursor.nOffset;
      int nMatched = 0;

      if (nCurEnd == 0 && run_prev_all_paras( run ))
      {
        run = run_prev_all_paras( run );
        para = run->para;
        nCurEnd = run->len;
      }

      while (run && ME_CharCompare( *get_text( run, nCurEnd - nMatched - 1 ),
                                    text[nLen - nMatched - 1], (flags & FR_MATCHCASE) ))
      {
        if ((flags & FR_WHOLEWORD) && iswalnum(wLastChar))
          break;

        nMatched++;
        if (nMatched == nLen)
        {
          ME_Run *prev_run = run;
          int nPrevEnd = nCurEnd;
          WCHAR wPrevChar;
          int nStart;

          /* Check to see if previous character is a whitespace */
          if (flags & FR_WHOLEWORD)
          {
            if (nPrevEnd - nMatched == 0)
            {
              prev_run = run_prev_all_paras( run );
              if (prev_run) nPrevEnd = prev_run->len + nMatched;
            }

            if (prev_run) wPrevChar = *get_text( prev_run, nPrevEnd - nMatched - 1 );
            else wPrevChar = ' ';

            if (iswalnum(wPrevChar))
              break;
          }

          nStart = para->nCharOfs + run->nCharOfs + nCurEnd - nMatched;
          if (chrgText)
          {
            chrgText->cpMin = nStart;
            chrgText->cpMax = nStart + nLen;
          }
          TRACE("found at %d-%d\n", nStart, nStart + nLen);
          return nStart;
        }
        if (nCurEnd - nMatched == 0)
        {
            if (run_prev_all_paras( run ))
            {
                run = run_prev_all_paras( run );
                para = run->para;
            }
          /* Don't care about pCurItem becoming NULL here; it's already taken
           * care of in the exterior loop condition */
          nCurEnd = run->len + nMatched;
        }
      }
      if (run)
        wLastChar = *get_text( run, nCurEnd - nMatched - 1 );
      else
        wLastChar = ' ';

      cursor.nOffset--;
      if (cursor.nOffset < 0)
      {
        if (run_prev_all_paras( cursor.run ) )
        {
          cursor.run = run_prev_all_paras( cursor.run );
          cursor.para = cursor.run->para;
          cursor.nOffset = cursor.run->len;
        }
        else
          cursor.run = NULL;
      }
    }
  }
  TRACE("not found\n");
  if (chrgText)
    chrgText->cpMin = chrgText->cpMax = -1;
  return -1;
}

static int ME_GetTextEx(ME_TextEditor *editor, GETTEXTEX *ex, LPARAM pText)
{
    int nChars;
    ME_Cursor start;

    if (!ex->cb || !pText) return 0;

    if (ex->flags & ~(GT_SELECTION | GT_USECRLF))
      FIXME("GETTEXTEX flags 0x%08x not supported\n", ex->flags & ~(GT_SELECTION | GT_USECRLF));

    if (ex->flags & GT_SELECTION)
    {
      int from, to;
      int nStartCur = ME_GetSelectionOfs(editor, &from, &to);
      start = editor->pCursors[nStartCur];
      nChars = to - from;
    }
    else
    {
      ME_SetCursorToStart(editor, &start);
      nChars = INT_MAX;
    }
    if (ex->codepage == CP_UNICODE)
    {
      return ME_GetTextW(editor, (LPWSTR)pText, ex->cb / sizeof(WCHAR) - 1,
                         &start, nChars, ex->flags & GT_USECRLF, FALSE);
    }
    else
    {
      /* potentially each char may be a CR, why calculate the exact value with O(N) when
        we can just take a bigger buffer? :)
        The above assumption still holds with CR/LF counters, since CR->CRLF expansion
        occurs only in richedit 2.0 mode, in which line breaks have only one CR
       */
      int crlfmul = (ex->flags & GT_USECRLF) ? 2 : 1;
      DWORD buflen;
      LPWSTR buffer;
      LRESULT rc;

      buflen = min(crlfmul * nChars, ex->cb - 1);
      buffer = heap_alloc((buflen + 1) * sizeof(WCHAR));

      nChars = ME_GetTextW(editor, buffer, buflen, &start, nChars, ex->flags & GT_USECRLF, FALSE);
      rc = WideCharToMultiByte(ex->codepage, 0, buffer, nChars + 1,
                               (LPSTR)pText, ex->cb, ex->lpDefaultChar, ex->lpUsedDefChar);
      if (rc) rc--; /* do not count 0 terminator */

      heap_free(buffer);
      return rc;
    }
}

static int ME_GetTextRange(ME_TextEditor *editor, WCHAR *strText,
                           const ME_Cursor *start, int nLen, BOOL unicode)
{
    if (!strText) return 0;
    if (unicode) {
      return ME_GetTextW(editor, strText, INT_MAX, start, nLen, FALSE, FALSE);
    } else {
      int nChars;
      WCHAR *p = heap_alloc((nLen+1) * sizeof(*p));
      if (!p) return 0;
      nChars = ME_GetTextW(editor, p, nLen, start, nLen, FALSE, FALSE);
      WideCharToMultiByte(CP_ACP, 0, p, nChars+1, (char *)strText,
                          nLen+1, NULL, NULL);
      heap_free(p);
      return nChars;
    }
}

int set_selection( ME_TextEditor *editor, int to, int from )
{
    int end;

    TRACE("%d - %d\n", to, from );

    if (!editor->bHideSelection) ME_InvalidateSelection( editor );
    end = set_selection_cursors( editor, to, from );
    if (!editor->bHideSelection) ME_InvalidateSelection( editor );
    update_caret( editor );
    ME_SendSelChange( editor );

    return end;
}

typedef struct tagME_GlobalDestStruct
{
  HGLOBAL hData;
  int nLength;
} ME_GlobalDestStruct;

static DWORD CALLBACK ME_ReadFromHGLOBALUnicode(DWORD_PTR dwCookie, LPBYTE lpBuff, LONG cb, LONG *pcb)
{
  ME_GlobalDestStruct *pData = (ME_GlobalDestStruct *)dwCookie;
  int i;
  WORD *pSrc, *pDest;

  cb = cb >> 1;
  pDest = (WORD *)lpBuff;
  pSrc = GlobalLock(pData->hData);
  for (i = 0; i<cb && pSrc[pData->nLength+i]; i++) {
    pDest[i] = pSrc[pData->nLength+i];
  }
  pData->nLength += i;
  *pcb = 2*i;
  GlobalUnlock(pData->hData);
  return 0;
}

static DWORD CALLBACK ME_ReadFromHGLOBALRTF(DWORD_PTR dwCookie, LPBYTE lpBuff, LONG cb, LONG *pcb)
{
  ME_GlobalDestStruct *pData = (ME_GlobalDestStruct *)dwCookie;
  int i;
  BYTE *pSrc, *pDest;

  pDest = lpBuff;
  pSrc = GlobalLock(pData->hData);
  for (i = 0; i<cb && pSrc[pData->nLength+i]; i++) {
    pDest[i] = pSrc[pData->nLength+i];
  }
  pData->nLength += i;
  *pcb = i;
  GlobalUnlock(pData->hData);
  return 0;
}

static HRESULT paste_rtf(ME_TextEditor *editor, FORMATETC *fmt, STGMEDIUM *med)
{
    EDITSTREAM es;
    ME_GlobalDestStruct gds;
    HRESULT hr;

    gds.hData = med->u.hGlobal;
    gds.nLength = 0;
    es.dwCookie = (DWORD_PTR)&gds;
    es.pfnCallback = ME_ReadFromHGLOBALRTF;
    hr = ME_StreamIn( editor, SF_RTF | SFF_SELECTION, &es, FALSE ) == 0 ? E_FAIL : S_OK;
    ReleaseStgMedium( med );
    return hr;
}

static HRESULT paste_text(ME_TextEditor *editor, FORMATETC *fmt, STGMEDIUM *med)
{
    EDITSTREAM es;
    ME_GlobalDestStruct gds;
    HRESULT hr;

    gds.hData = med->u.hGlobal;
    gds.nLength = 0;
    es.dwCookie = (DWORD_PTR)&gds;
    es.pfnCallback = ME_ReadFromHGLOBALUnicode;
    hr = ME_StreamIn( editor, SF_TEXT | SF_UNICODE | SFF_SELECTION, &es, FALSE ) == 0 ? E_FAIL : S_OK;
    ReleaseStgMedium( med );
    return hr;
}

static HRESULT paste_emf(ME_TextEditor *editor, FORMATETC *fmt, STGMEDIUM *med)
{
    HRESULT hr;
    SIZEL sz = {0, 0};

    hr = insert_static_object( editor, med->u.hEnhMetaFile, NULL, &sz );
    if (SUCCEEDED(hr))
    {
        ME_CommitUndo( editor );
        ME_UpdateRepaint( editor, FALSE );
    }
    else
        ReleaseStgMedium( med );

    return hr;
}

static struct paste_format
{
    FORMATETC fmt;
    HRESULT (*paste)(ME_TextEditor *, FORMATETC *, STGMEDIUM *);
    const WCHAR *name;
} paste_formats[] =
{
    {{ -1,             NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL }, paste_rtf, L"Rich Text Format" },
    {{ CF_UNICODETEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL }, paste_text },
    {{ CF_ENHMETAFILE, NULL, DVASPECT_CONTENT, -1, TYMED_ENHMF },   paste_emf },
    {{ 0 }}
};

static void init_paste_formats(void)
{
    struct paste_format *format;
    static int done;

    if (!done)
    {
        for (format = paste_formats; format->fmt.cfFormat; format++)
        {
            if (format->name)
                format->fmt.cfFormat = RegisterClipboardFormatW( format->name );
        }
        done = 1;
    }
}

static BOOL paste_special(ME_TextEditor *editor, UINT cf, REPASTESPECIAL *ps, BOOL check_only)
{
    HRESULT hr;
    STGMEDIUM med;
    struct paste_format *format;
    IDataObject *data;

    /* Protect read-only edit control from modification */
    if (editor->styleFlags & ES_READONLY)
    {
        if (!check_only)
            MessageBeep(MB_ICONERROR);
        return FALSE;
    }

    init_paste_formats();

    if (ps && ps->dwAspect != DVASPECT_CONTENT)
        FIXME("Ignoring aspect %x\n", ps->dwAspect);

    hr = OleGetClipboard( &data );
    if (hr != S_OK) return FALSE;

    if (cf == CF_TEXT) cf = CF_UNICODETEXT;

    hr = S_FALSE;
    for (format = paste_formats; format->fmt.cfFormat; format++)
    {
        if (cf && cf != format->fmt.cfFormat) continue;
        hr = IDataObject_QueryGetData( data, &format->fmt );
        if (hr == S_OK)
        {
            if (!check_only)
            {
                hr = IDataObject_GetData( data, &format->fmt, &med );
                if (hr != S_OK) goto done;
                hr = format->paste( editor, &format->fmt, &med );
            }
            break;
        }
    }

done:
    IDataObject_Release( data );

    return hr == S_OK;
}

static HRESULT editor_copy( ME_TextEditor *editor, ME_Cursor *start, int chars, IDataObject **data_out )
{
    IDataObject *data = NULL;
    HRESULT hr = S_OK;

    if (editor->lpOleCallback)
    {
        CHARRANGE range;
        range.cpMin = ME_GetCursorOfs( start );
        range.cpMax = range.cpMin + chars;
        hr = IRichEditOleCallback_GetClipboardData( editor->lpOleCallback, &range, RECO_COPY, &data );
    }

    if (FAILED( hr ) || !data)
        hr = ME_GetDataObject( editor, start, chars, &data );

    if (SUCCEEDED( hr ))
    {
        if (data_out)
            *data_out = data;
        else
        {
            hr = OleSetClipboard( data );
            IDataObject_Release( data );
        }
    }

    return hr;
}

HRESULT editor_copy_or_cut( ME_TextEditor *editor, BOOL cut, ME_Cursor *start, int count,
                            IDataObject **data_out )
{
    HRESULT hr;

    if (cut && (editor->styleFlags & ES_READONLY))
    {
        return E_ACCESSDENIED;
    }

    hr = editor_copy( editor, start, count, data_out );
    if (SUCCEEDED(hr) && cut)
    {
        ME_InternalDeleteText( editor, start, count, FALSE );
        ME_CommitUndo( editor );
        ME_UpdateRepaint( editor, TRUE );
    }
    return hr;
}

static BOOL copy_or_cut( ME_TextEditor *editor, BOOL cut )
{
    HRESULT hr;
    int offs, count;
    int start_cursor = ME_GetSelectionOfs( editor, &offs, &count );
    ME_Cursor *sel_start = &editor->pCursors[start_cursor];

    if (editor->cPasswordMask) return FALSE;

    count -= offs;
    hr = editor_copy_or_cut( editor, cut, sel_start, count, NULL );
    if (FAILED( hr )) MessageBeep( MB_ICONERROR );

    return SUCCEEDED( hr );
}

/* helper to send a msg filter notification */
static BOOL
ME_FilterEvent(ME_TextEditor *editor, UINT msg, WPARAM* wParam, LPARAM* lParam)
{
    MSGFILTER msgf;

    if (!editor->hWnd || !editor->hwndParent) return FALSE;
    msgf.nmhdr.hwndFrom = editor->hWnd;
    msgf.nmhdr.idFrom = GetWindowLongW(editor->hWnd, GWLP_ID);
    msgf.nmhdr.code = EN_MSGFILTER;
    msgf.msg = msg;
    msgf.wParam = *wParam;
    msgf.lParam = *lParam;
    if (SendMessageW(editor->hwndParent, WM_NOTIFY, msgf.nmhdr.idFrom, (LPARAM)&msgf))
        return FALSE;
    *wParam = msgf.wParam;
    *lParam = msgf.lParam;
    msgf.wParam = *wParam;

    return TRUE;
}

static void ME_UpdateSelectionLinkAttribute(ME_TextEditor *editor)
{
  ME_Paragraph *start_para, *end_para;
  ME_Cursor *from, *to, start;
  int num_chars;

  if (!editor->AutoURLDetect_bEnable) return;

  ME_GetSelection(editor, &from, &to);

  /* Find paragraph previous to the one that contains start cursor */
  start_para = from->para;
  if (para_prev( start_para )) start_para = para_prev( start_para );

  /* Find paragraph that contains end cursor */
  end_para = para_next( to->para );

  start.para = start_para;
  start.run = para_first_run( start_para );
  start.nOffset = 0;
  num_chars = end_para->nCharOfs - start_para->nCharOfs;

  ME_UpdateLinkAttribute( editor, &start, num_chars );
}

static BOOL handle_enter(ME_TextEditor *editor)
{
    BOOL ctrl_is_down = GetKeyState(VK_CONTROL) & 0x8000;
    BOOL shift_is_down = GetKeyState(VK_SHIFT) & 0x8000;

    if (editor->bDialogMode)
    {
        if (ctrl_is_down)
            return TRUE;

        if (!(editor->styleFlags & ES_WANTRETURN))
        {
            if (editor->hwndParent)
            {
                DWORD dw;
                dw = SendMessageW(editor->hwndParent, DM_GETDEFID, 0, 0);
                if (HIWORD(dw) == DC_HASDEFID)
                {
                    HWND hwDefCtrl = GetDlgItem(editor->hwndParent, LOWORD(dw));
                    if (hwDefCtrl)
                    {
                        SendMessageW(editor->hwndParent, WM_NEXTDLGCTL, (WPARAM)hwDefCtrl, TRUE);
                        PostMessageW(hwDefCtrl, WM_KEYDOWN, VK_RETURN, 0);
                    }
                }
            }
            return TRUE;
        }
    }

    if (editor->styleFlags & ES_MULTILINE)
    {
        ME_Cursor cursor = editor->pCursors[0];
        ME_Paragraph *para = cursor.para;
        int from, to;
        ME_Style *style, *eop_style;

        if (editor->styleFlags & ES_READONLY)
        {
            MessageBeep(MB_ICONERROR);
            return TRUE;
        }

        ME_GetSelectionOfs(editor, &from, &to);
        if (editor->nTextLimit > ME_GetTextLength(editor) - (to-from))
        {
            if (!editor->bEmulateVersion10) /* v4.1 */
            {
                if (para->nFlags & MEPF_ROWEND)
                {
                    /* Add a new table row after this row. */
                    para = table_append_row( editor, para );
                    para = para_next( para );
                    editor->pCursors[0].para = para;
                    editor->pCursors[0].run = para_first_run( para );
                    editor->pCursors[0].nOffset = 0;
                    editor->pCursors[1] = editor->pCursors[0];
                    ME_CommitUndo(editor);
                    ME_UpdateRepaint(editor, FALSE);
                    return TRUE;
                }
                else if (para == editor->pCursors[1].para &&
                         cursor.nOffset + cursor.run->nCharOfs == 0 &&
                         para_prev( para ) && para_prev( para )->nFlags & MEPF_ROWSTART &&
                         !para_prev( para )->nCharOfs)
                {
                    /* Insert a newline before the table. */
                    para = para_prev( para );
                    para->nFlags &= ~MEPF_ROWSTART;
                    editor->pCursors[0].para = para;
                    editor->pCursors[0].run = para_first_run( para );
                    editor->pCursors[1] = editor->pCursors[0];
                    ME_InsertTextFromCursor( editor, 0, L"\r", 1, editor->pCursors[0].run->style );
                    para = editor_first_para( editor );
                    editor_set_default_para_fmt( editor, &para->fmt );
                    para->nFlags = 0;
                    para_mark_rewrap( editor, para );
                    editor->pCursors[0].para = para;
                    editor->pCursors[0].run = para_first_run( para );
                    editor->pCursors[1] = editor->pCursors[0];
                    para_next( para )->nFlags |= MEPF_ROWSTART;
                    ME_CommitCoalescingUndo(editor);
                    ME_UpdateRepaint(editor, FALSE);
                    return TRUE;
                }
            }
            else /* v1.0 - 3.0 */
            {
                ME_Paragraph *para = cursor.para;
                if (para_in_table( para ))
                {
                    if (cursor.run->nFlags & MERF_ENDPARA)
                    {
                        if (from == to)
                        {
                            ME_ContinueCoalescingTransaction(editor);
                            para = table_append_row( editor, para );
                            editor->pCursors[0].para = para;
                            editor->pCursors[0].run = para_first_run( para );
                            editor->pCursors[0].nOffset = 0;
                            editor->pCursors[1] = editor->pCursors[0];
                            ME_CommitCoalescingUndo(editor);
                            ME_UpdateRepaint(editor, FALSE);
                            return TRUE;
                        }
                    }
                    else
                    {
                        ME_ContinueCoalescingTransaction(editor);
                        if (cursor.run->nCharOfs + cursor.nOffset == 0 &&
                            para_prev( para ) && !para_in_table( para_prev( para ) ))
                        {
                            /* Insert newline before table */
                            cursor.run = para_end_run( para_prev( para ) );
                            if (cursor.run)
                            {
                                editor->pCursors[0].run = cursor.run;
                                editor->pCursors[0].para = para_prev( para );
                            }
                            editor->pCursors[0].nOffset = 0;
                            editor->pCursors[1] = editor->pCursors[0];
                            ME_InsertTextFromCursor( editor, 0, L"\r", 1, editor->pCursors[0].run->style );
                        }
                        else
                        {
                            editor->pCursors[1] = editor->pCursors[0];
                            para = table_append_row( editor, para );
                            editor->pCursors[0].para = para;
                            editor->pCursors[0].run = para_first_run( para );
                            editor->pCursors[0].nOffset = 0;
                            editor->pCursors[1] = editor->pCursors[0];
                        }
                        ME_CommitCoalescingUndo(editor);
                        ME_UpdateRepaint(editor, FALSE);
                        return TRUE;
                    }
                }
            }

            style = style_get_insert_style( editor, editor->pCursors );

            /* Normally the new eop style is the insert style, however in a list it is copied from the existing
            eop style (this prevents the list label style changing when the new eop is inserted).
            No extra ref is taken here on eop_style. */
            if (para->fmt.wNumbering)
                eop_style = para->eop_run->style;
            else
                eop_style = style;
            ME_ContinueCoalescingTransaction(editor);
            if (shift_is_down)
                ME_InsertEndRowFromCursor(editor, 0);
            else
                if (!editor->bEmulateVersion10)
                    ME_InsertTextFromCursor(editor, 0, L"\r", 1, eop_style);
                else
                    ME_InsertTextFromCursor(editor, 0, L"\r\n", 2, eop_style);
            ME_CommitCoalescingUndo(editor);
            SetCursor(NULL);

            ME_UpdateSelectionLinkAttribute(editor);
            ME_UpdateRepaint(editor, FALSE);
            ME_SaveTempStyle(editor, style); /* set the temp insert style for the new para */
            ME_ReleaseStyle(style);
        }
        return TRUE;
    }
    return FALSE;
}

static BOOL
ME_KeyDown(ME_TextEditor *editor, WORD nKey)
{
  BOOL ctrl_is_down = GetKeyState(VK_CONTROL) & 0x8000;
  BOOL shift_is_down = GetKeyState(VK_SHIFT) & 0x8000;

  if (editor->bMouseCaptured)
      return FALSE;
  if (nKey != VK_SHIFT && nKey != VK_CONTROL && nKey != VK_MENU)
      editor->nSelectionType = stPosition;

  switch (nKey)
  {
    case VK_LEFT:
    case VK_RIGHT:
    case VK_HOME:
    case VK_END:
        editor->nUDArrowX = -1;
        /* fall through */
    case VK_UP:
    case VK_DOWN:
    case VK_PRIOR:
    case VK_NEXT:
      ME_CommitUndo(editor); /* End coalesced undos for typed characters */
      ME_ArrowKey(editor, nKey, shift_is_down, ctrl_is_down);
      return TRUE;
    case VK_BACK:
    case VK_DELETE:
      editor->nUDArrowX = -1;
      /* FIXME backspace and delete aren't the same, they act different wrt paragraph style of the merged paragraph */
      if (editor->styleFlags & ES_READONLY)
        return FALSE;
      if (ME_IsSelection(editor))
      {
        ME_DeleteSelection(editor);
        ME_CommitUndo(editor);
      }
      else if (nKey == VK_DELETE)
      {
        /* Delete stops group typing.
         * (See MSDN remarks on EM_STOPGROUPTYPING message) */
        ME_DeleteTextAtCursor(editor, 1, 1);
        ME_CommitUndo(editor);
      }
      else if (ME_ArrowKey(editor, VK_LEFT, FALSE, FALSE))
      {
        BOOL bDeletionSucceeded;
        /* Backspace can be grouped for a single undo */
        ME_ContinueCoalescingTransaction(editor);
        bDeletionSucceeded = ME_DeleteTextAtCursor(editor, 1, 1);
        if (!bDeletionSucceeded && !editor->bEmulateVersion10) { /* v4.1 */
          /* Deletion was prevented so the cursor is moved back to where it was.
           * (e.g. this happens when trying to delete cell boundaries)
           */
          ME_ArrowKey(editor, VK_RIGHT, FALSE, FALSE);
        }
        ME_CommitCoalescingUndo(editor);
      }
      else
        return TRUE;
      table_move_from_row_start( editor );
      ME_UpdateSelectionLinkAttribute(editor);
      ME_UpdateRepaint(editor, FALSE);
      ME_SendRequestResize(editor, FALSE);
      return TRUE;
    case VK_RETURN:
      if (!editor->bEmulateVersion10)
          return handle_enter(editor);
      break;
    case VK_ESCAPE:
      if (editor->bDialogMode && editor->hwndParent)
        PostMessageW(editor->hwndParent, WM_CLOSE, 0, 0);
      return TRUE;
    case VK_TAB:
      if (editor->bDialogMode && editor->hwndParent)
        SendMessageW(editor->hwndParent, WM_NEXTDLGCTL, shift_is_down, 0);
      return TRUE;
    case 'A':
      if (ctrl_is_down)
      {
        set_selection( editor, 0, -1 );
        return TRUE;
      }
      break;
    case 'V':
      if (ctrl_is_down)
        return paste_special( editor, 0, NULL, FALSE );
      break;
    case 'C':
    case 'X':
      if (ctrl_is_down)
        return copy_or_cut(editor, nKey == 'X');
      break;
    case 'Z':
      if (ctrl_is_down)
      {
        ME_Undo(editor);
        return TRUE;
      }
      break;
    case 'Y':
      if (ctrl_is_down)
      {
        ME_Redo(editor);
        return TRUE;
      }
      break;

    default:
      if (nKey != VK_SHIFT && nKey != VK_CONTROL && nKey && nKey != VK_MENU)
          editor->nUDArrowX = -1;
      if (ctrl_is_down)
      {
        if (nKey == 'W')
        {
          CHARFORMAT2W chf;
          char buf[2048];
          chf.cbSize = sizeof(chf);
          
          ME_GetSelectionCharFormat(editor, &chf);
          ME_DumpStyleToBuf(&chf, buf);
          MessageBoxA(NULL, buf, "Style dump", MB_OK);
        }
        if (nKey == 'Q')
        {
          ME_CheckCharOffsets(editor);
        }
      }
  }
  return FALSE;
}

static LRESULT ME_Char(ME_TextEditor *editor, WPARAM charCode,
                       LPARAM flags, BOOL unicode)
{
  WCHAR wstr;

  if (editor->bMouseCaptured)
    return 0;

  if (editor->styleFlags & ES_READONLY)
  {
    MessageBeep(MB_ICONERROR);
    return 0; /* FIXME really 0 ? */
  }

  if (unicode)
      wstr = (WCHAR)charCode;
  else
  {
      CHAR charA = charCode;
      MultiByteToWideChar(CP_ACP, 0, &charA, 1, &wstr, 1);
  }

  if (editor->bEmulateVersion10 && wstr == '\r')
      handle_enter(editor);

  if ((unsigned)wstr >= ' ' || wstr == '\t')
  {
    ME_Cursor cursor = editor->pCursors[0];
    ME_Paragraph *para = cursor.para;
    int from, to;
    BOOL ctrl_is_down = GetKeyState(VK_CONTROL) & 0x8000;
    ME_GetSelectionOfs(editor, &from, &to);
    if (wstr == '\t' &&
        /* v4.1 allows tabs to be inserted with ctrl key down */
        !(ctrl_is_down && !editor->bEmulateVersion10))
    {
      BOOL selected_row = FALSE;

      if (ME_IsSelection(editor) &&
          cursor.run->nCharOfs + cursor.nOffset == 0 &&
          to == ME_GetCursorOfs(&editor->pCursors[0]) && para_prev( para ))
      {
        para = para_prev( para );
        selected_row = TRUE;
      }
      if (para_in_table( para ))
      {
        table_handle_tab( editor, selected_row );
        ME_CommitUndo(editor);
        return 0;
      }
    }
    else if (!editor->bEmulateVersion10) /* v4.1 */
    {
      if (para->nFlags & MEPF_ROWEND)
      {
        if (from == to)
        {
          para = para_next( para );
          if (para->nFlags & MEPF_ROWSTART) para = para_next( para );
          editor->pCursors[0].para = para;
          editor->pCursors[0].run = para_first_run( para );
          editor->pCursors[0].nOffset = 0;
          editor->pCursors[1] = editor->pCursors[0];
        }
      }
    }
    else /* v1.0 - 3.0 */
    {
      if (para_in_table( para ) && cursor.run->nFlags & MERF_ENDPARA && from == to)
      {
        /* Text should not be inserted at the end of the table. */
        MessageBeep(-1);
        return 0;
      }
    }
    /* FIXME maybe it would make sense to call EM_REPLACESEL instead ? */
    /* WM_CHAR is restricted to nTextLimit */
    if(editor->nTextLimit > ME_GetTextLength(editor) - (to-from))
    {
      ME_Style *style = style_get_insert_style( editor, editor->pCursors );
      ME_ContinueCoalescingTransaction(editor);
      ME_InsertTextFromCursor(editor, 0, &wstr, 1, style);
      ME_ReleaseStyle(style);
      ME_CommitCoalescingUndo(editor);
      ITextHost_TxSetCursor(editor->texthost, NULL, FALSE);
    }

    ME_UpdateSelectionLinkAttribute(editor);
    ME_UpdateRepaint(editor, FALSE);
  }
  return 0;
}

/* Process the message and calculate the new click count.
 *
 * returns: The click count if it is mouse down event, else returns 0. */
static int ME_CalculateClickCount(ME_TextEditor *editor, UINT msg, WPARAM wParam,
                                  LPARAM lParam)
{
    static int clickNum = 0;
    if (msg < WM_MOUSEFIRST || msg > WM_MOUSELAST)
        return 0;

    if ((msg == WM_LBUTTONDBLCLK) ||
        (msg == WM_RBUTTONDBLCLK) ||
        (msg == WM_MBUTTONDBLCLK) ||
        (msg == WM_XBUTTONDBLCLK))
    {
        msg -= (WM_LBUTTONDBLCLK - WM_LBUTTONDOWN);
    }

    if ((msg == WM_LBUTTONDOWN) ||
        (msg == WM_RBUTTONDOWN) ||
        (msg == WM_MBUTTONDOWN) ||
        (msg == WM_XBUTTONDOWN))
    {
        static MSG prevClickMsg;
        MSG clickMsg;
        /* Compare the editor instead of the hwnd so that the this
         * can still be done for windowless richedit controls. */
        clickMsg.hwnd = (HWND)editor;
        clickMsg.message = msg;
        clickMsg.wParam = wParam;
        clickMsg.lParam = lParam;
        clickMsg.time = GetMessageTime();
        clickMsg.pt.x = (short)LOWORD(lParam);
        clickMsg.pt.y = (short)HIWORD(lParam);
        if ((clickNum != 0) &&
            (clickMsg.message == prevClickMsg.message) &&
            (clickMsg.hwnd == prevClickMsg.hwnd) &&
            (clickMsg.wParam == prevClickMsg.wParam) &&
            (clickMsg.time - prevClickMsg.time < GetDoubleClickTime()) &&
            (abs(clickMsg.pt.x - prevClickMsg.pt.x) < GetSystemMetrics(SM_CXDOUBLECLK)/2) &&
            (abs(clickMsg.pt.y - prevClickMsg.pt.y) < GetSystemMetrics(SM_CYDOUBLECLK)/2))
        {
            clickNum++;
        } else {
            clickNum = 1;
        }
        prevClickMsg = clickMsg;
    } else {
        return 0;
    }
    return clickNum;
}

static BOOL is_link( ME_Run *run )
{
    return (run->style->fmt.dwMask & CFM_LINK) && (run->style->fmt.dwEffects & CFE_LINK);
}

static BOOL ME_SetCursor(ME_TextEditor *editor)
{
  ME_Cursor cursor;
  POINT pt;
  BOOL isExact;
  SCROLLBARINFO sbi;
  DWORD messagePos = GetMessagePos();
  pt.x = (short)LOWORD(messagePos);
  pt.y = (short)HIWORD(messagePos);

  if (editor->hWnd)
  {
    sbi.cbSize = sizeof(sbi);
    GetScrollBarInfo(editor->hWnd, OBJID_HSCROLL, &sbi);
    if (!(sbi.rgstate[0] & (STATE_SYSTEM_INVISIBLE|STATE_SYSTEM_OFFSCREEN)) &&
        PtInRect(&sbi.rcScrollBar, pt))
    {
        ITextHost_TxSetCursor(editor->texthost,
                              LoadCursorW(NULL, (WCHAR*)IDC_ARROW), FALSE);
        return TRUE;
    }
    sbi.cbSize = sizeof(sbi);
    GetScrollBarInfo(editor->hWnd, OBJID_VSCROLL, &sbi);
    if (!(sbi.rgstate[0] & (STATE_SYSTEM_INVISIBLE|STATE_SYSTEM_OFFSCREEN)) &&
        PtInRect(&sbi.rcScrollBar, pt))
    {
        ITextHost_TxSetCursor(editor->texthost,
                              LoadCursorW(NULL, (WCHAR*)IDC_ARROW), FALSE);
        return TRUE;
    }
  }
  ITextHost_TxScreenToClient(editor->texthost, &pt);

  if (editor->nSelectionType == stLine && editor->bMouseCaptured) {
      ITextHost_TxSetCursor(editor->texthost, hLeft, FALSE);
      return TRUE;
  }
  if (!editor->bEmulateVersion10 /* v4.1 */ &&
      pt.y < editor->rcFormat.top &&
      pt.x < editor->rcFormat.left)
  {
      ITextHost_TxSetCursor(editor->texthost, hLeft, FALSE);
      return TRUE;
  }
  if (pt.y < editor->rcFormat.top || pt.y > editor->rcFormat.bottom)
  {
      if (editor->bEmulateVersion10) /* v1.0 - 3.0 */
          ITextHost_TxSetCursor(editor->texthost,
                                LoadCursorW(NULL, (WCHAR*)IDC_ARROW), FALSE);
      else /* v4.1 */
          ITextHost_TxSetCursor(editor->texthost,
                                LoadCursorW(NULL, (WCHAR*)IDC_IBEAM), TRUE);
      return TRUE;
  }
  if (pt.x < editor->rcFormat.left)
  {
      ITextHost_TxSetCursor(editor->texthost, hLeft, FALSE);
      return TRUE;
  }
  ME_CharFromPos(editor, pt.x, pt.y, &cursor, &isExact);
  if (isExact)
  {
      ME_Run *run = cursor.run;

      if (is_link( run ))
      {
          ITextHost_TxSetCursor(editor->texthost,
                                LoadCursorW(NULL, (WCHAR*)IDC_HAND),
                                FALSE);
          return TRUE;
      }

      if (ME_IsSelection(editor))
      {
          int selStart, selEnd;
          int offset = ME_GetCursorOfs(&cursor);

          ME_GetSelectionOfs(editor, &selStart, &selEnd);
          if (selStart <= offset && selEnd >= offset) {
              ITextHost_TxSetCursor(editor->texthost,
                                    LoadCursorW(NULL, (WCHAR*)IDC_ARROW),
                                    FALSE);
              return TRUE;
          }
      }
  }
  ITextHost_TxSetCursor(editor->texthost,
                        LoadCursorW(NULL, (WCHAR*)IDC_IBEAM), TRUE);
  return TRUE;
}

static void ME_SetDefaultFormatRect(ME_TextEditor *editor)
{
  ITextHost_TxGetClientRect(editor->texthost, &editor->rcFormat);
  editor->rcFormat.top += editor->exStyleFlags & WS_EX_CLIENTEDGE ? 1 : 0;
  editor->rcFormat.left += 1 + editor->selofs;
  editor->rcFormat.right -= 1;
}

static LONG ME_GetSelectionType(ME_TextEditor *editor)
{
    LONG sel_type = SEL_EMPTY;
    LONG start, end;

    ME_GetSelectionOfs(editor, &start, &end);
    if (start == end)
        sel_type = SEL_EMPTY;
    else
    {
        LONG object_count = 0, character_count = 0;
        int i;

        for (i = 0; i < end - start; i++)
        {
            ME_Cursor cursor;

            cursor_from_char_ofs( editor, start + i, &cursor );
            if (cursor.run->reobj) object_count++;
            else character_count++;
            if (character_count >= 2 && object_count >= 2)
                return (SEL_TEXT | SEL_MULTICHAR | SEL_OBJECT | SEL_MULTIOBJECT);
        }
        if (character_count)
        {
            sel_type |= SEL_TEXT;
            if (character_count >= 2)
                sel_type |= SEL_MULTICHAR;
        }
        if (object_count)
        {
            sel_type |= SEL_OBJECT;
            if (object_count >= 2)
                sel_type |= SEL_MULTIOBJECT;
        }
    }
    return sel_type;
}

static BOOL ME_ShowContextMenu(ME_TextEditor *editor, int x, int y)
{
  CHARRANGE selrange;
  HMENU menu;
  int seltype;

  if(!editor->lpOleCallback || !editor->hWnd)
    return FALSE;
  ME_GetSelectionOfs(editor, &selrange.cpMin, &selrange.cpMax);
  seltype = ME_GetSelectionType(editor);
  if(SUCCEEDED(IRichEditOleCallback_GetContextMenu(editor->lpOleCallback, seltype, NULL, &selrange, &menu)))
  {
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, x, y, 0, editor->hwndParent, NULL);
    DestroyMenu(menu);
  }
  return TRUE;
}

ME_TextEditor *ME_MakeEditor(ITextHost *texthost, BOOL bEmulateVersion10)
{
  ME_TextEditor *ed = heap_alloc(sizeof(*ed));
  int i;
  DWORD props;
  LONG selbarwidth;

  ed->hWnd = NULL;
  ed->hwndParent = NULL;
  ed->sizeWindow.cx = ed->sizeWindow.cy = 0;
  ed->texthost = texthost;
  ed->reOle = NULL;
  ed->bEmulateVersion10 = bEmulateVersion10;
  ed->styleFlags = 0;
  ed->exStyleFlags = 0;
  ed->total_rows = 0;
  ITextHost_TxGetPropertyBits(texthost,
                              (TXTBIT_RICHTEXT|TXTBIT_MULTILINE|
                               TXTBIT_READONLY|TXTBIT_USEPASSWORD|
                               TXTBIT_HIDESELECTION|TXTBIT_SAVESELECTION|
                               TXTBIT_AUTOWORDSEL|TXTBIT_VERTICAL|
                               TXTBIT_WORDWRAP|TXTBIT_DISABLEDRAG),
                              &props);
  ITextHost_TxGetScrollBars(texthost, &ed->styleFlags);
  ed->styleFlags &= (WS_VSCROLL|WS_HSCROLL|ES_AUTOVSCROLL|
                     ES_AUTOHSCROLL|ES_DISABLENOSCROLL);
  ed->pBuffer = ME_MakeText();
  ed->nZoomNumerator = ed->nZoomDenominator = 0;
  ed->nAvailWidth = 0; /* wrap to client area */
  list_init( &ed->style_list );
  ME_MakeFirstParagraph(ed);
  /* The four cursors are for:
   * 0 - The position where the caret is shown
   * 1 - The anchored end of the selection (for normal selection)
   * 2 & 3 - The anchored start and end respectively for word, line,
   * or paragraph selection.
   */
  ed->nCursors = 4;
  ed->pCursors = heap_alloc(ed->nCursors * sizeof(*ed->pCursors));
  ME_SetCursorToStart(ed, &ed->pCursors[0]);
  ed->pCursors[1] = ed->pCursors[0];
  ed->pCursors[2] = ed->pCursors[0];
  ed->pCursors[3] = ed->pCursors[1];
  ed->nLastTotalLength = ed->nTotalLength = 0;
  ed->nLastTotalWidth = ed->nTotalWidth = 0;
  ed->nUDArrowX = -1;
  ed->rgbBackColor = -1;
  ed->hbrBackground = GetSysColorBrush(COLOR_WINDOW);
  ed->nEventMask = 0;
  ed->nModifyStep = 0;
  ed->nTextLimit = TEXT_LIMIT_DEFAULT;
  list_init( &ed->undo_stack );
  list_init( &ed->redo_stack );
  ed->nUndoStackSize = 0;
  ed->nUndoLimit = STACK_SIZE_DEFAULT;
  ed->nUndoMode = umAddToUndo;
  ed->nParagraphs = 1;
  ed->nLastSelStart = ed->nLastSelEnd = 0;
  ed->last_sel_start_para = ed->last_sel_end_para = ed->pCursors[0].para;
  ed->bHideSelection = FALSE;
  ed->pfnWordBreak = NULL;
  ed->lpOleCallback = NULL;
  ed->mode = TM_MULTILEVELUNDO | TM_MULTICODEPAGE;
  ed->mode |= (props & TXTBIT_RICHTEXT) ? TM_RICHTEXT : TM_PLAINTEXT;
  ed->AutoURLDetect_bEnable = FALSE;
  ed->bHaveFocus = FALSE;
  ed->bDialogMode = FALSE;
  ed->bMouseCaptured = FALSE;
  ed->caret_hidden = FALSE;
  ed->caret_height = 0;
  for (i=0; i<HFONT_CACHE_SIZE; i++)
  {
    ed->pFontCache[i].nRefs = 0;
    ed->pFontCache[i].nAge = 0;
    ed->pFontCache[i].hFont = NULL;
  }

  ME_CheckCharOffsets(ed);
  SetRectEmpty(&ed->rcFormat);
  ed->bDefaultFormatRect = TRUE;
  ITextHost_TxGetSelectionBarWidth(ed->texthost, &selbarwidth);
  if (selbarwidth) {
    /* FIXME: Convert selbarwidth from HIMETRIC to pixels */
    ed->selofs = SELECTIONBAR_WIDTH;
    ed->styleFlags |= ES_SELECTIONBAR;
  } else {
    ed->selofs = 0;
  }
  ed->nSelectionType = stPosition;

  ed->cPasswordMask = 0;
  if (props & TXTBIT_USEPASSWORD)
    ITextHost_TxGetPasswordChar(texthost, &ed->cPasswordMask);

  if (props & TXTBIT_AUTOWORDSEL)
    ed->styleFlags |= ECO_AUTOWORDSELECTION;
  if (props & TXTBIT_MULTILINE) {
    ed->styleFlags |= ES_MULTILINE;
    ed->bWordWrap = (props & TXTBIT_WORDWRAP) != 0;
  } else {
    ed->bWordWrap = FALSE;
  }
  if (props & TXTBIT_READONLY)
    ed->styleFlags |= ES_READONLY;
  if (!(props & TXTBIT_HIDESELECTION))
    ed->styleFlags |= ES_NOHIDESEL;
  if (props & TXTBIT_SAVESELECTION)
    ed->styleFlags |= ES_SAVESEL;
  if (props & TXTBIT_VERTICAL)
    ed->styleFlags |= ES_VERTICAL;
  if (props & TXTBIT_DISABLEDRAG)
    ed->styleFlags |= ES_NOOLEDRAGDROP;

  ed->notified_cr.cpMin = ed->notified_cr.cpMax = 0;

  /* Default scrollbar information */
  ed->vert_si.cbSize = sizeof(SCROLLINFO);
  ed->vert_si.nMin = 0;
  ed->vert_si.nMax = 0;
  ed->vert_si.nPage = 0;
  ed->vert_si.nPos = 0;

  ed->horz_si.cbSize = sizeof(SCROLLINFO);
  ed->horz_si.nMin = 0;
  ed->horz_si.nMax = 0;
  ed->horz_si.nPage = 0;
  ed->horz_si.nPos = 0;

  ed->wheel_remain = 0;

  list_init( &ed->reobj_list );
  OleInitialize(NULL);

  return ed;
}

void ME_DestroyEditor(ME_TextEditor *editor)
{
  ME_DisplayItem *p = editor->pBuffer->pFirst, *pNext = NULL;
  ME_Style *s, *cursor2;
  int i;

  ME_ClearTempStyle(editor);
  ME_EmptyUndoStack(editor);
  editor->pBuffer->pFirst = NULL;
  while(p)
  {
    pNext = p->next;
    if (p->type == diParagraph)
      para_destroy( editor, &p->member.para );
    else
      ME_DestroyDisplayItem(p);
    p = pNext;
  }

  LIST_FOR_EACH_ENTRY_SAFE( s, cursor2, &editor->style_list, ME_Style, entry )
      ME_DestroyStyle( s );

  ME_ReleaseStyle(editor->pBuffer->pDefaultStyle);
  for (i=0; i<HFONT_CACHE_SIZE; i++)
  {
    if (editor->pFontCache[i].hFont)
      DeleteObject(editor->pFontCache[i].hFont);
  }
  if (editor->rgbBackColor != -1)
    DeleteObject(editor->hbrBackground);
  if(editor->lpOleCallback)
    IRichEditOleCallback_Release(editor->lpOleCallback);
  ITextHost_Release(editor->texthost);
  if (editor->reOle)
  {
    IUnknown_Release(editor->reOle);
    editor->reOle = NULL;
  }
  OleUninitialize();

  heap_free(editor->pBuffer);
  heap_free(editor->pCursors);
  heap_free(editor);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("\n");
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
      DisableThreadLibraryCalls(hinstDLL);
      me_heap = HeapCreate (0, 0x10000, 0);
      if (!ME_RegisterEditorClass(hinstDLL)) return FALSE;
      hLeft = LoadCursorW(hinstDLL, MAKEINTRESOURCEW(OCR_REVERSE));
      LookupInit();
      break;

    case DLL_PROCESS_DETACH:
      if (lpvReserved) break;
      UnregisterClassW(RICHEDIT_CLASS20W, 0);
      UnregisterClassW(MSFTEDIT_CLASS, 0);
      UnregisterClassA(RICHEDIT_CLASS20A, 0);
      UnregisterClassA("RichEdit50A", 0);
      if (ME_ListBoxRegistered)
          UnregisterClassW(L"REListBox20W", 0);
      if (ME_ComboBoxRegistered)
          UnregisterClassW(L"REComboBox20W", 0);
      LookupCleanup();
      HeapDestroy (me_heap);
      release_typelib();
      break;
    }
    return TRUE;
}

static inline int get_default_line_height( ME_TextEditor *editor )
{
    int height = 0;

    if (editor->pBuffer && editor->pBuffer->pDefaultStyle)
        height = editor->pBuffer->pDefaultStyle->tm.tmHeight;
    if (height <= 0) height = 24;

    return height;
}

static inline int calc_wheel_change( int *remain, int amount_per_click )
{
    int change = amount_per_click * (float)*remain / WHEEL_DELTA;
    *remain -= WHEEL_DELTA * change / amount_per_click;
    return change;
}

static const char * const edit_messages[] = {
  "EM_GETSEL",
  "EM_SETSEL",
  "EM_GETRECT",
  "EM_SETRECT",
  "EM_SETRECTNP",
  "EM_SCROLL",
  "EM_LINESCROLL",
  "EM_SCROLLCARET",
  "EM_GETMODIFY",
  "EM_SETMODIFY",
  "EM_GETLINECOUNT",
  "EM_LINEINDEX",
  "EM_SETHANDLE",
  "EM_GETHANDLE",
  "EM_GETTHUMB",
  "EM_UNKNOWN_BF",
  "EM_UNKNOWN_C0",
  "EM_LINELENGTH",
  "EM_REPLACESEL",
  "EM_UNKNOWN_C3",
  "EM_GETLINE",
  "EM_LIMITTEXT",
  "EM_CANUNDO",
  "EM_UNDO",
  "EM_FMTLINES",
  "EM_LINEFROMCHAR",
  "EM_UNKNOWN_CA",
  "EM_SETTABSTOPS",
  "EM_SETPASSWORDCHAR",
  "EM_EMPTYUNDOBUFFER",
  "EM_GETFIRSTVISIBLELINE",
  "EM_SETREADONLY",
  "EM_SETWORDBREAKPROC",
  "EM_GETWORDBREAKPROC",
  "EM_GETPASSWORDCHAR",
  "EM_SETMARGINS",
  "EM_GETMARGINS",
  "EM_GETLIMITTEXT",
  "EM_POSFROMCHAR",
  "EM_CHARFROMPOS",
  "EM_SETIMESTATUS",
  "EM_GETIMESTATUS"
};

static const char * const richedit_messages[] = {
  "EM_CANPASTE",
  "EM_DISPLAYBAND",
  "EM_EXGETSEL",
  "EM_EXLIMITTEXT",
  "EM_EXLINEFROMCHAR",
  "EM_EXSETSEL",
  "EM_FINDTEXT",
  "EM_FORMATRANGE",
  "EM_GETCHARFORMAT",
  "EM_GETEVENTMASK",
  "EM_GETOLEINTERFACE",
  "EM_GETPARAFORMAT",
  "EM_GETSELTEXT",
  "EM_HIDESELECTION", 
  "EM_PASTESPECIAL",
  "EM_REQUESTRESIZE",
  "EM_SELECTIONTYPE",
  "EM_SETBKGNDCOLOR",
  "EM_SETCHARFORMAT",
  "EM_SETEVENTMASK",
  "EM_SETOLECALLBACK",
  "EM_SETPARAFORMAT",
  "EM_SETTARGETDEVICE",
  "EM_STREAMIN",
  "EM_STREAMOUT",
  "EM_GETTEXTRANGE",
  "EM_FINDWORDBREAK",
  "EM_SETOPTIONS",
  "EM_GETOPTIONS",
  "EM_FINDTEXTEX",
  "EM_GETWORDBREAKPROCEX",
  "EM_SETWORDBREAKPROCEX",
  "EM_SETUNDOLIMIT",
  "EM_UNKNOWN_USER_83",
  "EM_REDO",
  "EM_CANREDO",
  "EM_GETUNDONAME",
  "EM_GETREDONAME",
  "EM_STOPGROUPTYPING",
  "EM_SETTEXTMODE",
  "EM_GETTEXTMODE",
  "EM_AUTOURLDETECT",
  "EM_GETAUTOURLDETECT",
  "EM_SETPALETTE",
  "EM_GETTEXTEX",
  "EM_GETTEXTLENGTHEX",
  "EM_SHOWSCROLLBAR",
  "EM_SETTEXTEX",
  "EM_UNKNOWN_USER_98",
  "EM_UNKNOWN_USER_99",
  "EM_SETPUNCTUATION",
  "EM_GETPUNCTUATION",
  "EM_SETWORDWRAPMODE",
  "EM_GETWORDWRAPMODE",
  "EM_SETIMECOLOR",
  "EM_GETIMECOLOR",
  "EM_SETIMEOPTIONS",
  "EM_GETIMEOPTIONS",
  "EM_CONVPOSITION",
  "EM_UNKNOWN_USER_109",
  "EM_UNKNOWN_USER_110",
  "EM_UNKNOWN_USER_111",
  "EM_UNKNOWN_USER_112",
  "EM_UNKNOWN_USER_113",
  "EM_UNKNOWN_USER_114",
  "EM_UNKNOWN_USER_115",
  "EM_UNKNOWN_USER_116",
  "EM_UNKNOWN_USER_117",
  "EM_UNKNOWN_USER_118",
  "EM_UNKNOWN_USER_119",
  "EM_SETLANGOPTIONS",
  "EM_GETLANGOPTIONS",
  "EM_GETIMECOMPMODE",
  "EM_FINDTEXTW",
  "EM_FINDTEXTEXW",
  "EM_RECONVERSION",
  "EM_SETIMEMODEBIAS",
  "EM_GETIMEMODEBIAS"
};

static const char *
get_msg_name(UINT msg)
{
  if (msg >= EM_GETSEL && msg <= EM_CHARFROMPOS)
    return edit_messages[msg - EM_GETSEL];
  if (msg >= EM_CANPASTE && msg <= EM_GETIMEMODEBIAS)
    return richedit_messages[msg - EM_CANPASTE];
  return "";
}

static void ME_LinkNotify(ME_TextEditor *editor, UINT msg, WPARAM wParam, LPARAM lParam)
{
  int x,y;
  BOOL isExact;
  ME_Cursor cursor; /* The start of the clicked text. */
  ME_Run *run;
  ENLINK info;

  x = (short)LOWORD(lParam);
  y = (short)HIWORD(lParam);
  ME_CharFromPos(editor, x, y, &cursor, &isExact);
  if (!isExact) return;

  if (is_link( cursor.run ))
  { /* The clicked run has CFE_LINK set */
    info.nmhdr.hwndFrom = NULL;
    info.nmhdr.idFrom = 0;
    info.nmhdr.code = EN_LINK;
    info.msg = msg;
    info.wParam = wParam;
    info.lParam = lParam;
    cursor.nOffset = 0;

    /* find the first contiguous run with CFE_LINK set */
    info.chrg.cpMin = ME_GetCursorOfs(&cursor);
    run = cursor.run;
    while ((run = run_prev( run )) && is_link( run ))
        info.chrg.cpMin -= run->len;

    /* find the last contiguous run with CFE_LINK set */
    info.chrg.cpMax = ME_GetCursorOfs(&cursor) + cursor.run->len;
    run = cursor.run;
    while ((run = run_next( run )) && is_link( run ))
        info.chrg.cpMax += run->len;

    ITextHost_TxNotify(editor->texthost, info.nmhdr.code, &info);
  }
}

void ME_ReplaceSel(ME_TextEditor *editor, BOOL can_undo, const WCHAR *str, int len)
{
  int from, to, nStartCursor;
  ME_Style *style;

  nStartCursor = ME_GetSelectionOfs(editor, &from, &to);
  style = ME_GetSelectionInsertStyle(editor);
  ME_InternalDeleteText(editor, &editor->pCursors[nStartCursor], to-from, FALSE);
  ME_InsertTextFromCursor(editor, 0, str, len, style);
  ME_ReleaseStyle(style);
  /* drop temporary style if line end */
  /*
   * FIXME question: does abc\n mean: put abc,
   * clear temp style, put \n? (would require a change)
   */
  if (len>0 && str[len-1] == '\n')
    ME_ClearTempStyle(editor);
  ME_CommitUndo(editor);
  ME_UpdateSelectionLinkAttribute(editor);
  if (!can_undo)
    ME_EmptyUndoStack(editor);
  ME_UpdateRepaint(editor, FALSE);
}

static void ME_SetText(ME_TextEditor *editor, void *text, BOOL unicode)
{
  LONG codepage = unicode ? CP_UNICODE : CP_ACP;
  int textLen;

  LPWSTR wszText = ME_ToUnicode(codepage, text, &textLen);
  ME_InsertTextFromCursor(editor, 0, wszText, textLen, editor->pBuffer->pDefaultStyle);
  ME_EndToUnicode(codepage, wszText);
}

static LRESULT ME_WmCreate(ME_TextEditor *editor, LPARAM lParam, BOOL unicode)
{
  CREATESTRUCTW *createW = (CREATESTRUCTW*)lParam;
  CREATESTRUCTA *createA = (CREATESTRUCTA*)lParam;
  void *text = NULL;
  INT max;

  if (lParam)
    text = unicode ? (void*)createW->lpszName : (void*)createA->lpszName;

  ME_SetDefaultFormatRect(editor);

  max = (editor->styleFlags & ES_DISABLENOSCROLL) ? 1 : 0;
  if (~editor->styleFlags & ES_DISABLENOSCROLL || editor->styleFlags & WS_VSCROLL)
    ITextHost_TxSetScrollRange(editor->texthost, SB_VERT, 0, max, TRUE);

  if (~editor->styleFlags & ES_DISABLENOSCROLL || editor->styleFlags & WS_HSCROLL)
    ITextHost_TxSetScrollRange(editor->texthost, SB_HORZ, 0, max, TRUE);

  if (editor->styleFlags & ES_DISABLENOSCROLL)
  {
    if (editor->styleFlags & WS_VSCROLL)
    {
      ITextHost_TxEnableScrollBar(editor->texthost, SB_VERT, ESB_DISABLE_BOTH);
      ITextHost_TxShowScrollBar(editor->texthost, SB_VERT, TRUE);
    }
    if (editor->styleFlags & WS_HSCROLL)
    {
      ITextHost_TxEnableScrollBar(editor->texthost, SB_HORZ, ESB_DISABLE_BOTH);
      ITextHost_TxShowScrollBar(editor->texthost, SB_HORZ, TRUE);
    }
  }

  if (text)
  {
    ME_SetText(editor, text, unicode);
    ME_SetCursorToStart(editor, &editor->pCursors[0]);
    ME_SetCursorToStart(editor, &editor->pCursors[1]);
  }

  ME_CommitUndo(editor);
  ME_WrapMarkedParagraphs(editor);
  update_caret(editor);
  return 0;
}

static LRESULT handle_EM_SETCHARFORMAT( ME_TextEditor *editor, WPARAM flags, const CHARFORMAT2W *fmt_in )
{
    CHARFORMAT2W fmt;
    BOOL changed = TRUE;
    ME_Cursor start, end;

    if (!cfany_to_cf2w( &fmt, fmt_in )) return 0;

    if (flags & SCF_ALL)
    {
        if (editor->mode & TM_PLAINTEXT)
        {
            ME_SetDefaultCharFormat( editor, &fmt );
        }
        else
        {
            ME_SetCursorToStart( editor, &start );
            ME_SetCharFormat( editor, &start, NULL, &fmt );
            editor->nModifyStep = 1;
        }
    }
    else if (flags & SCF_SELECTION)
    {
        if (editor->mode & TM_PLAINTEXT) return 0;
        if (flags & SCF_WORD)
        {
            end = editor->pCursors[0];
            ME_MoveCursorWords( editor, &end, +1 );
            start = end;
            ME_MoveCursorWords( editor, &start, -1 );
            ME_SetCharFormat( editor, &start, &end, &fmt );
        }
        changed = ME_IsSelection( editor );
        ME_SetSelectionCharFormat( editor, &fmt );
        if (changed) editor->nModifyStep = 1;
    }
    else /* SCF_DEFAULT */
    {
        ME_SetDefaultCharFormat( editor, &fmt );
    }

    ME_CommitUndo( editor );
    if (changed)
    {
        ME_WrapMarkedParagraphs( editor );
        ME_UpdateScrollBar( editor );
    }
    return 1;
}

#define UNSUPPORTED_MSG(e) \
  case e:                  \
    FIXME(#e ": stub\n");  \
    *phresult = S_FALSE;   \
    return 0;

/* Handle messages for windowless and windowed richedit controls.
 *
 * The LRESULT that is returned is a return value for window procs,
 * and the phresult parameter is the COM return code needed by the
 * text services interface. */
LRESULT ME_HandleMessage(ME_TextEditor *editor, UINT msg, WPARAM wParam,
                         LPARAM lParam, BOOL unicode, HRESULT* phresult)
{
  *phresult = S_OK;

  switch(msg) {

  UNSUPPORTED_MSG(EM_DISPLAYBAND)
  UNSUPPORTED_MSG(EM_FINDWORDBREAK)
  UNSUPPORTED_MSG(EM_FMTLINES)
  UNSUPPORTED_MSG(EM_FORMATRANGE)
  UNSUPPORTED_MSG(EM_GETBIDIOPTIONS)
  UNSUPPORTED_MSG(EM_GETEDITSTYLE)
  UNSUPPORTED_MSG(EM_GETIMECOMPMODE)
  UNSUPPORTED_MSG(EM_GETIMESTATUS)
  UNSUPPORTED_MSG(EM_SETIMESTATUS)
  UNSUPPORTED_MSG(EM_GETLANGOPTIONS)
  UNSUPPORTED_MSG(EM_GETREDONAME)
  UNSUPPORTED_MSG(EM_GETTYPOGRAPHYOPTIONS)
  UNSUPPORTED_MSG(EM_GETUNDONAME)
  UNSUPPORTED_MSG(EM_GETWORDBREAKPROCEX)
  UNSUPPORTED_MSG(EM_SETBIDIOPTIONS)
  UNSUPPORTED_MSG(EM_SETEDITSTYLE)
  UNSUPPORTED_MSG(EM_SETLANGOPTIONS)
  UNSUPPORTED_MSG(EM_SETMARGINS)
  UNSUPPORTED_MSG(EM_SETPALETTE)
  UNSUPPORTED_MSG(EM_SETTABSTOPS)
  UNSUPPORTED_MSG(EM_SETTYPOGRAPHYOPTIONS)
  UNSUPPORTED_MSG(EM_SETWORDBREAKPROCEX)

/* Messages specific to Richedit controls */

  case EM_STREAMIN:
   return ME_StreamIn(editor, wParam, (EDITSTREAM*)lParam, TRUE);
  case EM_STREAMOUT:
   return ME_StreamOut(editor, wParam, (EDITSTREAM *)lParam);
  case WM_GETDLGCODE:
  {
    UINT code = DLGC_WANTCHARS|DLGC_WANTTAB|DLGC_WANTARROWS;

    if (lParam)
      editor->bDialogMode = TRUE;
    if (editor->styleFlags & ES_MULTILINE)
      code |= DLGC_WANTMESSAGE;
    if (!(editor->styleFlags & ES_SAVESEL))
      code |= DLGC_HASSETSEL;
    return code;
  }
  case EM_EMPTYUNDOBUFFER:
    ME_EmptyUndoStack(editor);
    return 0;
  case EM_GETSEL:
  {
    /* Note: wParam/lParam can be NULL */
    UINT from, to;
    PUINT pfrom = wParam ? (PUINT)wParam : &from;
    PUINT pto = lParam ? (PUINT)lParam : &to;
    ME_GetSelectionOfs(editor, (int *)pfrom, (int *)pto);
    if ((*pfrom|*pto) & 0xFFFF0000)
      return -1;
    return MAKELONG(*pfrom,*pto);
  }
  case EM_EXGETSEL:
  {
    CHARRANGE *pRange = (CHARRANGE *)lParam;
    ME_GetSelectionOfs(editor, &pRange->cpMin, &pRange->cpMax);
    TRACE("EM_EXGETSEL = (%d,%d)\n", pRange->cpMin, pRange->cpMax);
    return 0;
  }
  case EM_SETUNDOLIMIT:
  {
    if ((int)wParam < 0)
      editor->nUndoLimit = STACK_SIZE_DEFAULT;
    else
      editor->nUndoLimit = min(wParam, STACK_SIZE_MAX);
    /* Setting a max stack size keeps wine from getting killed 
      for hogging memory. Windows allocates all this memory at once, so
      no program would realistically set a value above our maximum. */
    return editor->nUndoLimit;
  }
  case EM_CANUNDO:
    return !list_empty( &editor->undo_stack );
  case EM_CANREDO:
    return !list_empty( &editor->redo_stack );
  case WM_UNDO: /* FIXME: actually not the same */
  case EM_UNDO:
    return ME_Undo(editor);
  case EM_REDO:
    return ME_Redo(editor);
  case EM_GETOPTIONS:
  {
    /* these flags are equivalent to the ES_* counterparts */
    DWORD mask = ECO_VERTICAL | ECO_AUTOHSCROLL | ECO_AUTOVSCROLL |
                 ECO_NOHIDESEL | ECO_READONLY | ECO_WANTRETURN | ECO_SELECTIONBAR;
    DWORD settings = editor->styleFlags & mask;

    return settings;
  }
  case EM_SETFONTSIZE:
  {
      CHARFORMAT2W cf;
      LONG tmp_size, size;
      BOOL is_increase = ((LONG)wParam > 0);

      if (editor->mode & TM_PLAINTEXT)
          return FALSE;

      cf.cbSize = sizeof(cf);
      cf.dwMask = CFM_SIZE;
      ME_GetSelectionCharFormat(editor, &cf);
      tmp_size = (cf.yHeight / 20) + wParam;

      if (tmp_size <= 1)
          size = 1;
      else if (tmp_size > 12 && tmp_size < 28 && tmp_size % 2)
          size = tmp_size + (is_increase ? 1 : -1);
      else if (tmp_size > 28 && tmp_size < 36)
          size = is_increase ? 36 : 28;
      else if (tmp_size > 36 && tmp_size < 48)
          size = is_increase ? 48 : 36;
      else if (tmp_size > 48 && tmp_size < 72)
          size = is_increase ? 72 : 48;
      else if (tmp_size > 72 && tmp_size < 80)
          size = is_increase ? 80 : 72;
      else if (tmp_size > 80 && tmp_size < 1638)
          size = 10 * (is_increase ? (tmp_size / 10 + 1) : (tmp_size / 10));
      else if (tmp_size >= 1638)
          size = 1638;
      else
          size = tmp_size;

      cf.yHeight = size * 20; /*  convert twips to points */
      ME_SetSelectionCharFormat(editor, &cf);
      ME_CommitUndo(editor);
      ME_WrapMarkedParagraphs(editor);
      ME_UpdateScrollBar(editor);

      return TRUE;
  }
  case EM_SETOPTIONS:
  {
    /* these flags are equivalent to ES_* counterparts, except for
     * ECO_AUTOWORDSELECTION that doesn't have an ES_* counterpart,
     * but is still stored in editor->styleFlags. */
    const DWORD mask = ECO_VERTICAL | ECO_AUTOHSCROLL | ECO_AUTOVSCROLL |
                       ECO_NOHIDESEL | ECO_READONLY | ECO_WANTRETURN |
                       ECO_SELECTIONBAR | ECO_AUTOWORDSELECTION;
    DWORD settings = mask & editor->styleFlags;
    DWORD oldSettings = settings;
    DWORD changedSettings;

    switch(wParam)
    {
      case ECOOP_SET:
        settings = lParam;
        break;
      case ECOOP_OR:
        settings |= lParam;
        break;
      case ECOOP_AND:
        settings &= lParam;
        break;
      case ECOOP_XOR:
        settings ^= lParam;
    }
    changedSettings = oldSettings ^ settings;

    if (changedSettings) {
      editor->styleFlags = (editor->styleFlags & ~mask) | (settings & mask);

      if (changedSettings & ECO_SELECTIONBAR)
      {
        ITextHost_TxInvalidateRect(editor->texthost, &editor->rcFormat, TRUE);
        if (settings & ECO_SELECTIONBAR) {
          assert(!editor->selofs);
          editor->selofs = SELECTIONBAR_WIDTH;
          editor->rcFormat.left += editor->selofs;
        } else {
          editor->rcFormat.left -= editor->selofs;
          editor->selofs = 0;
        }
        ME_RewrapRepaint(editor);
      }

      if ((changedSettings & settings & ES_NOHIDESEL) && !editor->bHaveFocus)
          ME_InvalidateSelection( editor );

      if (changedSettings & settings & ECO_VERTICAL)
        FIXME("ECO_VERTICAL not implemented yet!\n");
      if (changedSettings & settings & ECO_AUTOHSCROLL)
        FIXME("ECO_AUTOHSCROLL not implemented yet!\n");
      if (changedSettings & settings & ECO_AUTOVSCROLL)
        FIXME("ECO_AUTOVSCROLL not implemented yet!\n");
      if (changedSettings & settings & ECO_WANTRETURN)
        FIXME("ECO_WANTRETURN not implemented yet!\n");
      if (changedSettings & settings & ECO_AUTOWORDSELECTION)
        FIXME("ECO_AUTOWORDSELECTION not implemented yet!\n");
    }

    return settings;
  }
  case EM_SETSEL:
  {
    return set_selection( editor, wParam, lParam );
  }
  case EM_SETSCROLLPOS:
  {
    POINT *point = (POINT *)lParam;
    ME_ScrollAbs(editor, point->x, point->y);
    return 0;
  }
  case EM_AUTOURLDETECT:
  {
    if (wParam==1 || wParam ==0)
    {
        editor->AutoURLDetect_bEnable = (BOOL)wParam;
        return 0;
    }
    return E_INVALIDARG;
  }
  case EM_GETAUTOURLDETECT:
  {
    return editor->AutoURLDetect_bEnable;
  }
  case EM_EXSETSEL:
  {
    CHARRANGE range = *(CHARRANGE *)lParam;

    return set_selection( editor, range.cpMin, range.cpMax );
  }
  case EM_SHOWSCROLLBAR:
  {
    DWORD flags;

    switch (wParam)
    {
      case SB_HORZ:
        flags = WS_HSCROLL;
        break;
      case SB_VERT:
        flags = WS_VSCROLL;
        break;
      case SB_BOTH:
        flags = WS_HSCROLL|WS_VSCROLL;
        break;
      default:
        return 0;
    }

    if (lParam) {
      editor->styleFlags |= flags;
      if (flags & WS_HSCROLL)
        ITextHost_TxShowScrollBar(editor->texthost, SB_HORZ,
                          editor->nTotalWidth > editor->sizeWindow.cx);
      if (flags & WS_VSCROLL)
        ITextHost_TxShowScrollBar(editor->texthost, SB_VERT,
                          editor->nTotalLength > editor->sizeWindow.cy);
    } else {
      editor->styleFlags &= ~flags;
      ITextHost_TxShowScrollBar(editor->texthost, wParam, FALSE);
    }
    return 0;
  }
  case EM_SETTEXTEX:
  {
    LPWSTR wszText;
    SETTEXTEX *pStruct = (SETTEXTEX *)wParam;
    int from, to, len;
    ME_Style *style;
    BOOL bRtf, bUnicode, bSelection, bUTF8;
    int oldModify = editor->nModifyStep;
    static const char utf8_bom[] = {0xef, 0xbb, 0xbf};

    if (!pStruct) return 0;

    /* If we detect ascii rtf at the start of the string,
     * we know it isn't unicode. */
    bRtf = (lParam && (!strncmp((char *)lParam, "{\\rtf", 5) ||
                         !strncmp((char *)lParam, "{\\urtf", 6)));
    bUnicode = !bRtf && pStruct->codepage == CP_UNICODE;
    bUTF8 = (lParam && (!strncmp((char *)lParam, utf8_bom, 3)));

    TRACE("EM_SETTEXTEX - %s, flags %d, cp %d\n",
          bUnicode ? debugstr_w((LPCWSTR)lParam) : debugstr_a((LPCSTR)lParam),
          pStruct->flags, pStruct->codepage);

    bSelection = (pStruct->flags & ST_SELECTION) != 0;
    if (bSelection) {
      int nStartCursor = ME_GetSelectionOfs(editor, &from, &to);
      style = ME_GetSelectionInsertStyle(editor);
      ME_InternalDeleteText(editor, &editor->pCursors[nStartCursor], to - from, FALSE);
    } else {
      ME_Cursor start;
      ME_SetCursorToStart(editor, &start);
      ME_InternalDeleteText(editor, &start, ME_GetTextLength(editor), FALSE);
      style = editor->pBuffer->pDefaultStyle;
    }

    if (bRtf) {
      ME_StreamInRTFString(editor, bSelection, (char *)lParam);
      if (bSelection) {
        /* FIXME: The length returned doesn't include the rtf control
         * characters, only the actual text. */
        len = lParam ? strlen((char *)lParam) : 0;
      }
    } else {
      if (bUTF8 && !bUnicode) {
        wszText = ME_ToUnicode(CP_UTF8, (void *)(lParam+3), &len);
        ME_InsertTextFromCursor(editor, 0, wszText, len, style);
        ME_EndToUnicode(CP_UTF8, wszText);
      } else {
        wszText = ME_ToUnicode(pStruct->codepage, (void *)lParam, &len);
        ME_InsertTextFromCursor(editor, 0, wszText, len, style);
        ME_EndToUnicode(pStruct->codepage, wszText);
      }
    }

    if (bSelection) {
      ME_ReleaseStyle(style);
      ME_UpdateSelectionLinkAttribute(editor);
    } else {
      ME_Cursor cursor;
      len = 1;
      ME_SetCursorToStart(editor, &cursor);
      ME_UpdateLinkAttribute(editor, &cursor, INT_MAX);
    }
    ME_CommitUndo(editor);
    if (!(pStruct->flags & ST_KEEPUNDO))
    {
      editor->nModifyStep = oldModify;
      ME_EmptyUndoStack(editor);
    }
    ME_UpdateRepaint(editor, FALSE);
    return len;
  }
  case EM_SELECTIONTYPE:
    return ME_GetSelectionType(editor);
  case EM_SETBKGNDCOLOR:
  {
    LRESULT lColor;
    if (editor->rgbBackColor != -1) {
      DeleteObject(editor->hbrBackground);
      lColor = editor->rgbBackColor;
    }
    else lColor = ITextHost_TxGetSysColor(editor->texthost, COLOR_WINDOW);

    if (wParam)
    {
      editor->rgbBackColor = -1;
      editor->hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    }
    else
    {
      editor->rgbBackColor = lParam;
      editor->hbrBackground = CreateSolidBrush(editor->rgbBackColor);
    }
    ITextHost_TxInvalidateRect(editor->texthost, NULL, TRUE);
    return lColor;
  }
  case EM_GETMODIFY:
    return editor->nModifyStep == 0 ? 0 : -1;
  case EM_SETMODIFY:
  {
    if (wParam)
      editor->nModifyStep = 1;
    else
      editor->nModifyStep = 0;
    
    return 0;
  }
  case EM_SETREADONLY:
  {
    if (wParam)
      editor->styleFlags |= ES_READONLY;
    else
      editor->styleFlags &= ~ES_READONLY;
    return 1;
  }
  case EM_SETEVENTMASK:
  {
    DWORD nOldMask = editor->nEventMask;
    
    editor->nEventMask = lParam;
    return nOldMask;
  }
  case EM_GETEVENTMASK:
    return editor->nEventMask;
  case EM_SETCHARFORMAT:
    return handle_EM_SETCHARFORMAT( editor, wParam, (CHARFORMAT2W *)lParam );
  case EM_GETCHARFORMAT:
  {
    CHARFORMAT2W tmp, *dst = (CHARFORMAT2W *)lParam;
    if (dst->cbSize != sizeof(CHARFORMATA) &&
        dst->cbSize != sizeof(CHARFORMATW) &&
        dst->cbSize != sizeof(CHARFORMAT2A) &&
        dst->cbSize != sizeof(CHARFORMAT2W))
      return 0;
    tmp.cbSize = sizeof(tmp);
    if (!wParam)
      ME_GetDefaultCharFormat(editor, &tmp);
    else
      ME_GetSelectionCharFormat(editor, &tmp);
    cf2w_to_cfany(dst, &tmp);
    return tmp.dwMask;
  }
  case EM_SETPARAFORMAT:
  {
    BOOL result = editor_set_selection_para_fmt( editor, (PARAFORMAT2 *)lParam );
    ME_WrapMarkedParagraphs(editor);
    ME_UpdateScrollBar(editor);
    ME_CommitUndo(editor);
    return result;
  }
  case EM_GETPARAFORMAT:
    editor_get_selection_para_fmt( editor, (PARAFORMAT2 *)lParam );
    return ((PARAFORMAT2 *)lParam)->dwMask;
  case EM_GETFIRSTVISIBLELINE:
  {
    ME_Paragraph *para = editor_first_para( editor );
    ME_Row *row;
    int y = editor->vert_si.nPos;
    int count = 0;

    while (para_next( para ))
    {
      if (y < para->pt.y + para->nHeight) break;
      count += para->nRows;
      para = para_next( para );
    }

    row = para_first_row( para );
    while (row)
    {
      if (y < para->pt.y + row->pt.y + row->nHeight) break;
      count++;
      row = row_next( row );
    }
    return count;
  }
  case EM_HIDESELECTION:
  {
     editor->bHideSelection = (wParam != 0);
     ME_InvalidateSelection(editor);
     return 0;
  }
  case EM_LINESCROLL:
  {
    if (!(editor->styleFlags & ES_MULTILINE))
      return FALSE;
    ME_ScrollDown( editor, lParam * get_default_line_height( editor ) );
    return TRUE;
  }
  case WM_CLEAR:
  {
    int from, to;
    int nStartCursor = ME_GetSelectionOfs(editor, &from, &to);
    ME_InternalDeleteText(editor, &editor->pCursors[nStartCursor], to-from, FALSE);
    ME_CommitUndo(editor);
    ME_UpdateRepaint(editor, TRUE);
    return 0;
  }
  case EM_REPLACESEL:
  {
    int len = 0;
    LONG codepage = unicode ? CP_UNICODE : CP_ACP;
    LPWSTR wszText = ME_ToUnicode(codepage, (void *)lParam, &len);

    TRACE("EM_REPLACESEL - %s\n", debugstr_w(wszText));

    ME_ReplaceSel(editor, !!wParam, wszText, len);
    ME_EndToUnicode(codepage, wszText);
    return len;
  }
  case EM_SCROLLCARET:
    editor_ensure_visible( editor, &editor->pCursors[0] );
    return 0;
  case WM_SETFONT:
  {
    LOGFONTW lf;
    CHARFORMAT2W fmt;
    HDC hDC;
    BOOL bRepaint = LOWORD(lParam);

    if (!wParam)
      wParam = (WPARAM)GetStockObject(SYSTEM_FONT);

    if (!GetObjectW((HGDIOBJ)wParam, sizeof(LOGFONTW), &lf))
      return 0;

    hDC = ITextHost_TxGetDC(editor->texthost);
    ME_CharFormatFromLogFont(hDC, &lf, &fmt);
    ITextHost_TxReleaseDC(editor->texthost, hDC);
    if (editor->mode & TM_RICHTEXT) {
      ME_Cursor start;
      ME_SetCursorToStart(editor, &start);
      ME_SetCharFormat(editor, &start, NULL, &fmt);
    }
    ME_SetDefaultCharFormat(editor, &fmt);

    ME_CommitUndo(editor);
    editor_mark_rewrap_all( editor );
    ME_WrapMarkedParagraphs(editor);
    ME_UpdateScrollBar(editor);
    if (bRepaint)
      ME_Repaint(editor);
    return 0;
  }
  case WM_SETTEXT:
  {
    ME_Cursor cursor;
    ME_SetCursorToStart(editor, &cursor);
    ME_InternalDeleteText(editor, &cursor, ME_GetTextLength(editor), FALSE);
    if (lParam)
    {
      TRACE("WM_SETTEXT lParam==%lx\n",lParam);
      if (!strncmp((char *)lParam, "{\\rtf", 5) ||
          !strncmp((char *)lParam, "{\\urtf", 6))
      {
        /* Undocumented: WM_SETTEXT supports RTF text */
        ME_StreamInRTFString(editor, 0, (char *)lParam);
      }
      else
        ME_SetText(editor, (void*)lParam, unicode);
    }
    else
      TRACE("WM_SETTEXT - NULL\n");
    ME_SetCursorToStart(editor, &cursor);
    ME_UpdateLinkAttribute(editor, &cursor, INT_MAX);
    set_selection_cursors(editor, 0, 0);
    editor->nModifyStep = 0;
    ME_CommitUndo(editor);
    ME_EmptyUndoStack(editor);
    ME_UpdateRepaint(editor, FALSE);
    return 1;
  }
  case EM_CANPASTE:
    return paste_special( editor, 0, NULL, TRUE );
  case WM_PASTE:
  case WM_MBUTTONDOWN:
    wParam = 0;
    lParam = 0;
    /* fall through */
  case EM_PASTESPECIAL:
    paste_special( editor, wParam, (REPASTESPECIAL *)lParam, FALSE );
    return 0;
  case WM_CUT:
  case WM_COPY:
    copy_or_cut(editor, msg == WM_CUT);
    return 0;
  case WM_GETTEXTLENGTH:
  {
    GETTEXTLENGTHEX how;

    /* CR/LF conversion required in 2.0 mode, verbatim in 1.0 mode */
    how.flags = GTL_CLOSE | (editor->bEmulateVersion10 ? 0 : GTL_USECRLF) | GTL_NUMCHARS;
    how.codepage = unicode ? CP_UNICODE : CP_ACP;
    return ME_GetTextLengthEx(editor, &how);
  }
  case EM_GETTEXTLENGTHEX:
    return ME_GetTextLengthEx(editor, (GETTEXTLENGTHEX *)wParam);
  case WM_GETTEXT:
  {
    GETTEXTEX ex;
    ex.cb = wParam * (unicode ? sizeof(WCHAR) : sizeof(CHAR));
    ex.flags = GT_USECRLF;
    ex.codepage = unicode ? CP_UNICODE : CP_ACP;
    ex.lpDefaultChar = NULL;
    ex.lpUsedDefChar = NULL;
    return ME_GetTextEx(editor, &ex, lParam);
  }
  case EM_GETTEXTEX:
    return ME_GetTextEx(editor, (GETTEXTEX*)wParam, lParam);
  case EM_GETSELTEXT:
  {
    int nFrom, nTo, nStartCur = ME_GetSelectionOfs(editor, &nFrom, &nTo);
    ME_Cursor *from = &editor->pCursors[nStartCur];
    return ME_GetTextRange(editor, (WCHAR *)lParam, from,
                           nTo - nFrom, unicode);
  }
  case EM_GETSCROLLPOS:
  {
    POINT *point = (POINT *)lParam;
    point->x = editor->horz_si.nPos;
    point->y = editor->vert_si.nPos;
    /* 16-bit scaled value is returned as stored in scrollinfo */
    if (editor->horz_si.nMax > 0xffff)
      point->x = MulDiv(point->x, 0xffff, editor->horz_si.nMax);
    if (editor->vert_si.nMax > 0xffff)
      point->y = MulDiv(point->y, 0xffff, editor->vert_si.nMax);
    return 1;
  }
  case EM_GETTEXTRANGE:
  {
    TEXTRANGEW *rng = (TEXTRANGEW *)lParam;
    ME_Cursor start;
    int nStart = rng->chrg.cpMin;
    int nEnd = rng->chrg.cpMax;
    int textlength = ME_GetTextLength(editor);

    TRACE("EM_GETTEXTRANGE min=%d max=%d unicode=%d textlength=%d\n",
          rng->chrg.cpMin, rng->chrg.cpMax, unicode, textlength);
    if (nStart < 0) return 0;
    if ((nStart == 0 && nEnd == -1) || nEnd > textlength)
      nEnd = textlength;
    if (nStart >= nEnd) return 0;

    cursor_from_char_ofs( editor, nStart, &start );
    return ME_GetTextRange(editor, rng->lpstrText, &start, nEnd - nStart, unicode);
  }
  case EM_GETLINE:
  {
    ME_Row *row;
    ME_Run *run;
    const unsigned int nMaxChars = *(WORD *) lParam;
    unsigned int nCharsLeft = nMaxChars;
    char *dest = (char *) lParam;
    BOOL wroteNull = FALSE;
    ME_Cursor start, end;

    TRACE("EM_GETLINE: row=%d, nMaxChars=%d (%s)\n", (int) wParam, nMaxChars,
          unicode ? "Unicode" : "Ansi");

    row = row_from_row_number( editor, wParam );
    if (row == NULL) return 0;

    row_first_cursor( row, &start );
    row_end_cursor( row, &end, TRUE );
    run = start.run;
    while (nCharsLeft)
    {
      WCHAR *str;
      unsigned int nCopy;
      int ofs = (run == start.run) ? start.nOffset : 0;
      int len = (run == end.run) ? end.nOffset : run->len;

      str = get_text( run, ofs );
      nCopy = min( nCharsLeft, len );

      if (unicode)
        memcpy(dest, str, nCopy * sizeof(WCHAR));
      else
        nCopy = WideCharToMultiByte(CP_ACP, 0, str, nCopy, dest,
                                    nCharsLeft, NULL, NULL);
      dest += nCopy * (unicode ? sizeof(WCHAR) : 1);
      nCharsLeft -= nCopy;
      if (run == end.run) break;
      run = row_next_run( row, run );
    }

    /* append line termination, space allowing */
    if (nCharsLeft > 0)
    {
      if (unicode)
        *((WCHAR *)dest) = '\0';
      else
        *dest = '\0';
      nCharsLeft--;
      wroteNull = TRUE;
    }

    TRACE("EM_GETLINE: got %u characters\n", nMaxChars - nCharsLeft);
    return nMaxChars - nCharsLeft - (wroteNull ? 1 : 0);
  }
  case EM_GETLINECOUNT:
  {
    int count = editor->total_rows;
    ME_Run *prev_run, *last_run;

    last_run = para_end_run( para_prev( editor_end_para( editor ) ) );
    prev_run = run_prev_all_paras( last_run );

    if (editor->bEmulateVersion10 && prev_run && last_run->nCharOfs == 0 &&
        prev_run->len == 1 && *get_text( prev_run, 0 ) == '\r')
    {
      /* In 1.0 emulation, the last solitary \r at the very end of the text
         (if one exists) is NOT a line break.
         FIXME: this is an ugly hack. This should have a more regular model. */
      count--;
    }

    count = max(1, count);
    TRACE("EM_GETLINECOUNT: count==%d\n", count);
    return count;
  }
  case EM_LINEFROMCHAR:
  {
    if (wParam == -1) wParam = ME_GetCursorOfs( editor->pCursors + 1 );
    return row_number_from_char_ofs( editor, wParam );
  }
  case EM_EXLINEFROMCHAR:
  {
    if (lParam == -1) lParam = ME_GetCursorOfs( editor->pCursors + 1 );
    return row_number_from_char_ofs( editor, lParam );
  }
  case EM_LINEINDEX:
  {
    ME_Row *row;
    ME_Cursor cursor;
    int ofs;
    
    if (wParam == -1) row = row_from_cursor( editor->pCursors );
    else row = row_from_row_number( editor, wParam );
    if (!row) return -1;

    row_first_cursor( row, &cursor );
    ofs = ME_GetCursorOfs( &cursor );
    TRACE( "EM_LINEINDEX: nCharOfs==%d\n", ofs );
    return ofs;
  }
  case EM_LINELENGTH:
  {
    ME_Row *row;
    int start_ofs, end_ofs;
    ME_Cursor cursor;

    if (wParam > ME_GetTextLength(editor))
      return 0;
    if (wParam == -1)
    {
      FIXME("EM_LINELENGTH: returning number of unselected characters on lines with selection unsupported.\n");
      return 0;
    }
    cursor_from_char_ofs( editor, wParam, &cursor );
    row = row_from_cursor( &cursor );
    row_first_cursor( row, &cursor );
    start_ofs = ME_GetCursorOfs( &cursor );
    row_end_cursor( row, &cursor, FALSE );
    end_ofs = ME_GetCursorOfs( &cursor );
    TRACE( "EM_LINELENGTH(%ld)==%d\n", wParam, end_ofs - start_ofs );
    return end_ofs - start_ofs;
  }
  case EM_EXLIMITTEXT:
  {
    if ((int)lParam < 0)
     return 0;
    if (lParam == 0)
      editor->nTextLimit = 65536;
    else
      editor->nTextLimit = (int) lParam;
    return 0;
  }
  case EM_LIMITTEXT:
  {
    if (wParam == 0)
      editor->nTextLimit = 65536;
    else
      editor->nTextLimit = (int) wParam;
    return 0;
  }
  case EM_GETLIMITTEXT:
  {
    return editor->nTextLimit;
  }
  case EM_FINDTEXT:
  {
    LRESULT r;
    if(!unicode){
      FINDTEXTA *ft = (FINDTEXTA *)lParam;
      int nChars = MultiByteToWideChar(CP_ACP, 0, ft->lpstrText, -1, NULL, 0);
      WCHAR *tmp;

      if ((tmp = heap_alloc(nChars * sizeof(*tmp))) != NULL)
        MultiByteToWideChar(CP_ACP, 0, ft->lpstrText, -1, tmp, nChars);
      r = ME_FindText(editor, wParam, &ft->chrg, tmp, NULL);
      heap_free(tmp);
    }else{
      FINDTEXTW *ft = (FINDTEXTW *)lParam;
      r = ME_FindText(editor, wParam, &ft->chrg, ft->lpstrText, NULL);
    }
    return r;
  }
  case EM_FINDTEXTEX:
  {
    LRESULT r;
    if(!unicode){
      FINDTEXTEXA *ex = (FINDTEXTEXA *)lParam;
      int nChars = MultiByteToWideChar(CP_ACP, 0, ex->lpstrText, -1, NULL, 0);
      WCHAR *tmp;

      if ((tmp = heap_alloc(nChars * sizeof(*tmp))) != NULL)
        MultiByteToWideChar(CP_ACP, 0, ex->lpstrText, -1, tmp, nChars);
      r = ME_FindText(editor, wParam, &ex->chrg, tmp, &ex->chrgText);
      heap_free(tmp);
    }else{
      FINDTEXTEXW *ex = (FINDTEXTEXW *)lParam;
      r = ME_FindText(editor, wParam, &ex->chrg, ex->lpstrText, &ex->chrgText);
    }
    return r;
  }
  case EM_FINDTEXTW:
  {
    FINDTEXTW *ft = (FINDTEXTW *)lParam;
    return ME_FindText(editor, wParam, &ft->chrg, ft->lpstrText, NULL);
  }
  case EM_FINDTEXTEXW:
  {
    FINDTEXTEXW *ex = (FINDTEXTEXW *)lParam;
    return ME_FindText(editor, wParam, &ex->chrg, ex->lpstrText, &ex->chrgText);
  }
  case EM_GETZOOM:
    if (!wParam || !lParam)
      return FALSE;
    *(int *)wParam = editor->nZoomNumerator;
    *(int *)lParam = editor->nZoomDenominator;
    return TRUE;
  case EM_SETZOOM:
    return ME_SetZoom(editor, wParam, lParam);
  case EM_CHARFROMPOS:
  {
    ME_Cursor cursor;
    if (ME_CharFromPos(editor, ((POINTL *)lParam)->x, ((POINTL *)lParam)->y,
                       &cursor, NULL))
      return ME_GetCursorOfs(&cursor);
    else
      return -1;
  }
  case EM_POSFROMCHAR:
  {
    ME_Cursor cursor;
    int nCharOfs, nLength;
    POINTL pt = {0,0};

    nCharOfs = wParam;
    /* detect which API version we're dealing with */
    if (wParam >= 0x40000)
        nCharOfs = lParam;
    nLength = ME_GetTextLength(editor);
    nCharOfs = min(nCharOfs, nLength);
    nCharOfs = max(nCharOfs, 0);

    cursor_from_char_ofs( editor, nCharOfs, &cursor );
    pt.y = cursor.run->pt.y;
    pt.x = cursor.run->pt.x +
        ME_PointFromChar( editor, cursor.run, cursor.nOffset, TRUE );
    pt.y += cursor.para->pt.y + editor->rcFormat.top;
    pt.x += editor->rcFormat.left;

    pt.x -= editor->horz_si.nPos;
    pt.y -= editor->vert_si.nPos;

    if (wParam >= 0x40000) *(POINTL *)wParam = pt;

    return (wParam >= 0x40000) ? 0 : MAKELONG( pt.x, pt.y );
  }
  case WM_CREATE:
    return ME_WmCreate(editor, lParam, unicode);
  case WM_DESTROY:
    ME_DestroyEditor(editor);
    return 0;
  case WM_SETCURSOR:
  {
    POINT cursor_pos;
    if (wParam == (WPARAM)editor->hWnd && GetCursorPos(&cursor_pos) &&
        ScreenToClient(editor->hWnd, &cursor_pos))
      ME_LinkNotify(editor, msg, 0, MAKELPARAM(cursor_pos.x, cursor_pos.y));
    return ME_SetCursor(editor);
  }
  case WM_LBUTTONDBLCLK:
  case WM_LBUTTONDOWN:
  {
    ME_CommitUndo(editor); /* End coalesced undos for typed characters */
    if ((editor->nEventMask & ENM_MOUSEEVENTS) &&
        !ME_FilterEvent(editor, msg, &wParam, &lParam))
      return 0;
    ITextHost_TxSetFocus(editor->texthost);
    ME_LButtonDown(editor, (short)LOWORD(lParam), (short)HIWORD(lParam),
                   ME_CalculateClickCount(editor, msg, wParam, lParam));
    ITextHost_TxSetCapture(editor->texthost, TRUE);
    editor->bMouseCaptured = TRUE;
    ME_LinkNotify(editor, msg, wParam, lParam);
    if (!ME_SetCursor(editor)) goto do_default;
    break;
  }
  case WM_MOUSEMOVE:
    if ((editor->nEventMask & ENM_MOUSEEVENTS) &&
        !ME_FilterEvent(editor, msg, &wParam, &lParam))
      return 0;
    if (editor->bMouseCaptured)
      ME_MouseMove(editor, (short)LOWORD(lParam), (short)HIWORD(lParam));
    else
      ME_LinkNotify(editor, msg, wParam, lParam);
    /* Set cursor if mouse is captured, since WM_SETCURSOR won't be received. */
    if (editor->bMouseCaptured)
        ME_SetCursor(editor);
    break;
  case WM_LBUTTONUP:
    if (editor->bMouseCaptured) {
      ITextHost_TxSetCapture(editor->texthost, FALSE);
      editor->bMouseCaptured = FALSE;
    }
    if (editor->nSelectionType == stDocument)
      editor->nSelectionType = stPosition;
    if ((editor->nEventMask & ENM_MOUSEEVENTS) &&
        !ME_FilterEvent(editor, msg, &wParam, &lParam))
      return 0;
    else
    {
      ME_SetCursor(editor);
      ME_LinkNotify(editor, msg, wParam, lParam);
    }
    break;
  case WM_RBUTTONUP:
  case WM_RBUTTONDOWN:
  case WM_RBUTTONDBLCLK:
    ME_CommitUndo(editor); /* End coalesced undos for typed characters */
    if ((editor->nEventMask & ENM_MOUSEEVENTS) &&
        !ME_FilterEvent(editor, msg, &wParam, &lParam))
      return 0;
    ME_LinkNotify(editor, msg, wParam, lParam);
    goto do_default;
  case WM_CONTEXTMENU:
    if (!ME_ShowContextMenu(editor, (short)LOWORD(lParam), (short)HIWORD(lParam)))
      goto do_default;
    break;
  case WM_SETFOCUS:
    editor->bHaveFocus = TRUE;
    create_caret(editor);
    update_caret(editor);
    ME_SendOldNotify(editor, EN_SETFOCUS);
    if (!editor->bHideSelection && !(editor->styleFlags & ES_NOHIDESEL))
        ME_InvalidateSelection( editor );
    return 0;
  case WM_KILLFOCUS:
    ME_CommitUndo(editor); /* End coalesced undos for typed characters */
    editor->bHaveFocus = FALSE;
    editor->wheel_remain = 0;
    hide_caret(editor);
    DestroyCaret();
    ME_SendOldNotify(editor, EN_KILLFOCUS);
    if (!editor->bHideSelection && !(editor->styleFlags & ES_NOHIDESEL))
        ME_InvalidateSelection( editor );
    return 0;
  case WM_COMMAND:
    TRACE("editor wnd command = %d\n", LOWORD(wParam));
    return 0;
  case WM_KEYUP:
    if ((editor->nEventMask & ENM_KEYEVENTS) &&
        !ME_FilterEvent(editor, msg, &wParam, &lParam))
      return 0;
    goto do_default;
  case WM_KEYDOWN:
    if ((editor->nEventMask & ENM_KEYEVENTS) &&
        !ME_FilterEvent(editor, msg, &wParam, &lParam))
      return 0;
    if (ME_KeyDown(editor, LOWORD(wParam)))
      return 0;
    goto do_default;
  case WM_CHAR:
    if ((editor->nEventMask & ENM_KEYEVENTS) &&
        !ME_FilterEvent(editor, msg, &wParam, &lParam))
      return 0;
    return ME_Char(editor, wParam, lParam, unicode);
  case WM_UNICHAR:
    if (unicode)
    {
        if(wParam == UNICODE_NOCHAR) return TRUE;
        if(wParam <= 0x000fffff)
        {
            if(wParam > 0xffff) /* convert to surrogates */
            {
                wParam -= 0x10000;
                ME_Char(editor, (wParam >> 10) + 0xd800, 0, TRUE);
                ME_Char(editor, (wParam & 0x03ff) + 0xdc00, 0, TRUE);
            } else {
              ME_Char(editor, wParam, 0, TRUE);
            }
        }
        return 0;
    }
    break;
  case EM_STOPGROUPTYPING:
    ME_CommitUndo(editor); /* End coalesced undos for typed characters */
    return 0;
  case WM_HSCROLL:
  {
    const int scrollUnit = 7;

    switch(LOWORD(wParam))
    {
      case SB_LEFT:
        ME_ScrollAbs(editor, 0, 0);
        break;
      case SB_RIGHT:
        ME_ScrollAbs(editor,
                     editor->horz_si.nMax - (int)editor->horz_si.nPage,
                     editor->vert_si.nMax - (int)editor->vert_si.nPage);
        break;
      case SB_LINELEFT:
        ME_ScrollLeft(editor, scrollUnit);
        break;
      case SB_LINERIGHT:
        ME_ScrollRight(editor, scrollUnit);
        break;
      case SB_PAGELEFT:
        ME_ScrollLeft(editor, editor->sizeWindow.cx);
        break;
      case SB_PAGERIGHT:
        ME_ScrollRight(editor, editor->sizeWindow.cx);
        break;
      case SB_THUMBTRACK:
      case SB_THUMBPOSITION:
      {
        int pos = HIWORD(wParam);
        if (editor->horz_si.nMax > 0xffff)
          pos = MulDiv(pos, editor->horz_si.nMax, 0xffff);
        ME_HScrollAbs(editor, pos);
        break;
      }
    }
    break;
  }
  case EM_SCROLL: /* fall through */
  case WM_VSCROLL:
  {
    int origNPos;
    int lineHeight = get_default_line_height( editor );

    origNPos = editor->vert_si.nPos;

    switch(LOWORD(wParam))
    {
      case SB_TOP:
        ME_ScrollAbs(editor, 0, 0);
        break;
      case SB_BOTTOM:
        ME_ScrollAbs(editor,
                     editor->horz_si.nMax - (int)editor->horz_si.nPage,
                     editor->vert_si.nMax - (int)editor->vert_si.nPage);
        break;
      case SB_LINEUP:
        ME_ScrollUp(editor,lineHeight);
        break;
      case SB_LINEDOWN:
        ME_ScrollDown(editor,lineHeight);
        break;
      case SB_PAGEUP:
        ME_ScrollUp(editor,editor->sizeWindow.cy);
        break;
      case SB_PAGEDOWN:
        ME_ScrollDown(editor,editor->sizeWindow.cy);
        break;
      case SB_THUMBTRACK:
      case SB_THUMBPOSITION:
      {
        int pos = HIWORD(wParam);
        if (editor->vert_si.nMax > 0xffff)
          pos = MulDiv(pos, editor->vert_si.nMax, 0xffff);
        ME_VScrollAbs(editor, pos);
        break;
      }
    }
    if (msg == EM_SCROLL)
      return 0x00010000 | (((editor->vert_si.nPos - origNPos)/lineHeight) & 0xffff);
    break;
  }
  case WM_MOUSEWHEEL:
  {
    int delta;
    BOOL ctrl_is_down;

    if ((editor->nEventMask & ENM_MOUSEEVENTS) &&
        !ME_FilterEvent(editor, msg, &wParam, &lParam))
      return 0;

    ctrl_is_down = GetKeyState(VK_CONTROL) & 0x8000;

    delta = GET_WHEEL_DELTA_WPARAM(wParam);

    /* if scrolling changes direction, ignore left overs */
    if ((delta < 0 && editor->wheel_remain < 0) ||
        (delta > 0 && editor->wheel_remain > 0))
      editor->wheel_remain += delta;
    else
      editor->wheel_remain = delta;

    if (editor->wheel_remain)
    {
      if (ctrl_is_down) {
        int numerator;
        if (!editor->nZoomNumerator || !editor->nZoomDenominator)
        {
          numerator = 100;
        } else {
          numerator = editor->nZoomNumerator * 100 / editor->nZoomDenominator;
        }
        numerator += calc_wheel_change( &editor->wheel_remain, 10 );
        if (numerator >= 10 && numerator <= 500)
          ME_SetZoom(editor, numerator, 100);
      } else {
        UINT max_lines = 3;
        int lines = 0;

        SystemParametersInfoW( SPI_GETWHEELSCROLLLINES, 0, &max_lines, 0 );
        if (max_lines)
          lines = calc_wheel_change( &editor->wheel_remain, (int)max_lines );
        if (lines)
          ME_ScrollDown( editor, -lines * get_default_line_height( editor ) );
      }
    }
    break;
  }
  case EM_GETRECT:
  {
    *((RECT *)lParam) = editor->rcFormat;
    if (editor->bDefaultFormatRect)
      ((RECT *)lParam)->left -= editor->selofs;
    return 0;
  }
  case EM_SETRECT:
  case EM_SETRECTNP:
  {
    if (lParam)
    {
      int border = 0;
      RECT clientRect;
      RECT *rc = (RECT *)lParam;

      border = editor->exStyleFlags & WS_EX_CLIENTEDGE ? 1 : 0;
      ITextHost_TxGetClientRect(editor->texthost, &clientRect);
      if (wParam == 0)
      {
        editor->rcFormat.top = max(0, rc->top - border);
        editor->rcFormat.left = max(0, rc->left - border);
        editor->rcFormat.bottom = min(clientRect.bottom, rc->bottom);
        editor->rcFormat.right = min(clientRect.right, rc->right + border);
      } else if (wParam == 1) {
        /* MSDN incorrectly says a wParam value of 1 causes the
         * lParam rect to be used as a relative offset,
         * however, the tests show it just prevents min/max bound
         * checking. */
        editor->rcFormat.top = rc->top - border;
        editor->rcFormat.left = rc->left - border;
        editor->rcFormat.bottom = rc->bottom;
        editor->rcFormat.right = rc->right + border;
      } else {
        return 0;
      }
      editor->bDefaultFormatRect = FALSE;
    }
    else
    {
      ME_SetDefaultFormatRect(editor);
      editor->bDefaultFormatRect = TRUE;
    }
    editor_mark_rewrap_all( editor );
    ME_WrapMarkedParagraphs(editor);
    ME_UpdateScrollBar(editor);
    if (msg != EM_SETRECTNP)
      ME_Repaint(editor);
    return 0;
  }
  case EM_REQUESTRESIZE:
    ME_SendRequestResize(editor, TRUE);
    return 0;
  case WM_SETREDRAW:
    goto do_default;
  case WM_WINDOWPOSCHANGED:
  {
    RECT clientRect;
    WINDOWPOS *winpos = (WINDOWPOS *)lParam;

    if (winpos->flags & SWP_NOCLIENTSIZE) goto do_default;
    ITextHost_TxGetClientRect(editor->texthost, &clientRect);
    if (editor->bDefaultFormatRect) {
      ME_SetDefaultFormatRect(editor);
    } else {
      editor->rcFormat.right += clientRect.right - editor->prevClientRect.right;
      editor->rcFormat.bottom += clientRect.bottom - editor->prevClientRect.bottom;
    }
    editor->prevClientRect = clientRect;
    ME_RewrapRepaint(editor);
    goto do_default;
  }
  /* IME messages to make richedit controls IME aware */
  case WM_IME_SETCONTEXT:
  case WM_IME_CONTROL:
  case WM_IME_SELECT:
  case WM_IME_COMPOSITIONFULL:
    return 0;
  case WM_IME_STARTCOMPOSITION:
  {
    editor->imeStartIndex=ME_GetCursorOfs(&editor->pCursors[0]);
    ME_DeleteSelection(editor);
    ME_CommitUndo(editor);
    ME_UpdateRepaint(editor, FALSE);
    return 0;
  }
  case WM_IME_COMPOSITION:
  {
    HIMC hIMC;

    ME_Style *style = style_get_insert_style( editor, editor->pCursors );
    hIMC = ITextHost_TxImmGetContext(editor->texthost);
    ME_DeleteSelection(editor);
    ME_SaveTempStyle(editor, style);
    if (lParam & (GCS_RESULTSTR|GCS_COMPSTR))
    {
        LPWSTR lpCompStr = NULL;
        DWORD dwBufLen;
        DWORD dwIndex = lParam & GCS_RESULTSTR;
        if (!dwIndex)
          dwIndex = GCS_COMPSTR;

        dwBufLen = ImmGetCompositionStringW(hIMC, dwIndex, NULL, 0);
        lpCompStr = HeapAlloc(GetProcessHeap(),0,dwBufLen + sizeof(WCHAR));
        ImmGetCompositionStringW(hIMC, dwIndex, lpCompStr, dwBufLen);
        lpCompStr[dwBufLen/sizeof(WCHAR)] = 0;
        ME_InsertTextFromCursor(editor,0,lpCompStr,dwBufLen/sizeof(WCHAR),style);
        HeapFree(GetProcessHeap(), 0, lpCompStr);

        if (dwIndex == GCS_COMPSTR)
          set_selection_cursors(editor,editor->imeStartIndex,
                          editor->imeStartIndex + dwBufLen/sizeof(WCHAR));
    }
    ME_ReleaseStyle(style);
    ME_CommitUndo(editor);
    ME_UpdateRepaint(editor, FALSE);
    return 0;
  }
  case WM_IME_ENDCOMPOSITION:
  {
    ME_DeleteSelection(editor);
    editor->imeStartIndex=-1;
    return 0;
  }
  case EM_GETOLEINTERFACE:
  {
    if (!editor->reOle)
      if (!CreateIRichEditOle(NULL, editor, (LPVOID *)&editor->reOle))
        return 0;
    if (IUnknown_QueryInterface(editor->reOle, &IID_IRichEditOle, (LPVOID *)lParam) == S_OK)
      return 1;
    return 0;
  }
  case EM_GETPASSWORDCHAR:
  {
    return editor->cPasswordMask;
  }
  case EM_SETOLECALLBACK:
    if(editor->lpOleCallback)
      IRichEditOleCallback_Release(editor->lpOleCallback);
    editor->lpOleCallback = (IRichEditOleCallback*)lParam;
    if(editor->lpOleCallback)
      IRichEditOleCallback_AddRef(editor->lpOleCallback);
    return TRUE;
  case EM_GETWORDBREAKPROC:
    return (LRESULT)editor->pfnWordBreak;
  case EM_SETWORDBREAKPROC:
  {
    EDITWORDBREAKPROCW pfnOld = editor->pfnWordBreak;

    editor->pfnWordBreak = (EDITWORDBREAKPROCW)lParam;
    return (LRESULT)pfnOld;
  }
  case EM_GETTEXTMODE:
    return editor->mode;
  case EM_SETTEXTMODE:
  {
    int mask = 0;
    int changes = 0;

    if (ME_GetTextLength(editor) ||
        !list_empty( &editor->undo_stack ) || !list_empty( &editor->redo_stack ))
      return E_UNEXPECTED;

    /* Check for mutually exclusive flags in adjacent bits of wParam */
    if ((wParam & (TM_RICHTEXT | TM_MULTILEVELUNDO | TM_MULTICODEPAGE)) &
        (wParam & (TM_PLAINTEXT | TM_SINGLELEVELUNDO | TM_SINGLECODEPAGE)) << 1)
      return E_INVALIDARG;

    if (wParam & (TM_RICHTEXT | TM_PLAINTEXT))
    {
      mask |= TM_RICHTEXT | TM_PLAINTEXT;
      changes |= wParam & (TM_RICHTEXT | TM_PLAINTEXT);
      if (wParam & TM_PLAINTEXT) {
        /* Clear selection since it should be possible to select the
         * end of text run for rich text */
        ME_InvalidateSelection(editor);
        ME_SetCursorToStart(editor, &editor->pCursors[0]);
        editor->pCursors[1] = editor->pCursors[0];
        /* plain text can only have the default style. */
        ME_ClearTempStyle(editor);
        ME_AddRefStyle(editor->pBuffer->pDefaultStyle);
        ME_ReleaseStyle( editor->pCursors[0].run->style );
        editor->pCursors[0].run->style = editor->pBuffer->pDefaultStyle;
      }
    }
    /* FIXME: Currently no support for undo level and code page options */
    editor->mode = (editor->mode & ~mask) | changes;
    return 0;
  }
  case EM_SETPASSWORDCHAR:
  {
    editor->cPasswordMask = wParam;
    ME_RewrapRepaint(editor);
    return 0;
  }
  case EM_SETTARGETDEVICE:
    if (wParam == 0)
    {
      BOOL new = (lParam == 0 && (editor->styleFlags & ES_MULTILINE));
      if (editor->nAvailWidth || editor->bWordWrap != new)
      {
        editor->bWordWrap = new;
        editor->nAvailWidth = 0; /* wrap to client area */
        ME_RewrapRepaint(editor);
      }
    } else {
      int width = max(0, lParam);
      if ((editor->styleFlags & ES_MULTILINE) &&
          (!editor->bWordWrap || editor->nAvailWidth != width))
      {
        editor->nAvailWidth = width;
        editor->bWordWrap = TRUE;
        ME_RewrapRepaint(editor);
      }
      FIXME("EM_SETTARGETDEVICE doesn't use non-NULL target devices\n");
    }
    return TRUE;
  default:
  do_default:
    *phresult = S_FALSE;
    break;
  }
  return 0L;
}

static BOOL create_windowed_editor(HWND hwnd, CREATESTRUCTW *create, BOOL emulate_10)
{
    ITextHost *host = ME_CreateTextHost( hwnd, create, emulate_10 );
    ME_TextEditor *editor;

    if (!host) return FALSE;

    editor = ME_MakeEditor( host, emulate_10 );
    if (!editor)
    {
        ITextHost_Release( host );
        return FALSE;
    }

    editor->exStyleFlags = GetWindowLongW( hwnd, GWL_EXSTYLE );
    editor->styleFlags |= GetWindowLongW( hwnd, GWL_STYLE ) & ES_WANTRETURN;
    editor->hWnd = hwnd; /* FIXME: Remove editor's dependence on hWnd */
    editor->hwndParent = create->hwndParent;

    SetWindowLongPtrW( hwnd, 0, (LONG_PTR)editor );

    return TRUE;
}

static LRESULT RichEditWndProc_common(HWND hWnd, UINT msg, WPARAM wParam,
                                      LPARAM lParam, BOOL unicode)
{
  ME_TextEditor *editor;
  HRESULT hresult;
  LRESULT lresult = 0;

  TRACE("enter hwnd %p msg %04x (%s) %lx %lx, unicode %d\n",
        hWnd, msg, get_msg_name(msg), wParam, lParam, unicode);

  editor = (ME_TextEditor *)GetWindowLongPtrW(hWnd, 0);
  if (!editor)
  {
    if (msg == WM_NCCREATE)
    {
      CREATESTRUCTW *pcs = (CREATESTRUCTW *)lParam;

      TRACE("WM_NCCREATE: hWnd %p style 0x%08x\n", hWnd, pcs->style);
      return create_windowed_editor( hWnd, pcs, FALSE );
    }
    else
    {
      return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
  }

  switch (msg)
  {
    case WM_PAINT:
    {
      HDC hdc;
      RECT rc;
      PAINTSTRUCT ps;
      HBRUSH old_brush;

      update_caret(editor);
      hdc = BeginPaint(editor->hWnd, &ps);
      if (!editor->bEmulateVersion10 || (editor->nEventMask & ENM_UPDATE))
        ME_SendOldNotify(editor, EN_UPDATE);
      old_brush = SelectObject(hdc, editor->hbrBackground);

      /* Erase area outside of the formatting rectangle */
      if (ps.rcPaint.top < editor->rcFormat.top)
      {
        rc = ps.rcPaint;
        rc.bottom = editor->rcFormat.top;
        PatBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, PATCOPY);
        ps.rcPaint.top = editor->rcFormat.top;
      }
      if (ps.rcPaint.bottom > editor->rcFormat.bottom) {
        rc = ps.rcPaint;
        rc.top = editor->rcFormat.bottom;
        PatBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, PATCOPY);
        ps.rcPaint.bottom = editor->rcFormat.bottom;
      }
      if (ps.rcPaint.left < editor->rcFormat.left) {
        rc = ps.rcPaint;
        rc.right = editor->rcFormat.left;
        PatBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, PATCOPY);
        ps.rcPaint.left = editor->rcFormat.left;
      }
      if (ps.rcPaint.right > editor->rcFormat.right) {
        rc = ps.rcPaint;
        rc.left = editor->rcFormat.right;
        PatBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, PATCOPY);
        ps.rcPaint.right = editor->rcFormat.right;
      }

      ME_PaintContent(editor, hdc, &ps.rcPaint);
      SelectObject(hdc, old_brush);
      EndPaint(editor->hWnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
    {
      HDC hDC = (HDC)wParam;
      RECT rc;

      if (GetUpdateRect(editor->hWnd, &rc, TRUE))
        FillRect(hDC, &rc, editor->hbrBackground);
      return 1;
    }
    case EM_SETOPTIONS:
    {
      DWORD dwStyle;
      const DWORD mask = ECO_VERTICAL | ECO_AUTOHSCROLL | ECO_AUTOVSCROLL |
                         ECO_NOHIDESEL | ECO_READONLY | ECO_WANTRETURN |
                         ECO_SELECTIONBAR;
      lresult = ME_HandleMessage(editor, msg, wParam, lParam, unicode, &hresult);
      dwStyle = GetWindowLongW(hWnd, GWL_STYLE);
      dwStyle = (dwStyle & ~mask) | (lresult & mask);
      SetWindowLongW(hWnd, GWL_STYLE, dwStyle);
      return lresult;
    }
    case EM_SETREADONLY:
    {
      DWORD dwStyle;
      lresult = ME_HandleMessage(editor, msg, wParam, lParam, unicode, &hresult);
      dwStyle = GetWindowLongW(hWnd, GWL_STYLE);
      dwStyle &= ~ES_READONLY;
      if (wParam)
        dwStyle |= ES_READONLY;
      SetWindowLongW(hWnd, GWL_STYLE, dwStyle);
      return lresult;
    }
    default:
      lresult = ME_HandleMessage(editor, msg, wParam, lParam, unicode, &hresult);
  }

  if (hresult == S_FALSE)
    lresult = DefWindowProcW(hWnd, msg, wParam, lParam);

  TRACE("exit hwnd %p msg %04x (%s) %lx %lx, unicode %d -> %lu\n",
        hWnd, msg, get_msg_name(msg), wParam, lParam, unicode, lresult);

  return lresult;
}

static LRESULT WINAPI RichEditWndProcW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    BOOL unicode = TRUE;

    /* Under Win9x RichEdit20W returns ANSI strings, see the tests. */
    if (msg == WM_GETTEXT && (GetVersion() & 0x80000000))
        unicode = FALSE;

    return RichEditWndProc_common(hWnd, msg, wParam, lParam, unicode);
}

static LRESULT WINAPI RichEditWndProcA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return RichEditWndProc_common(hWnd, msg, wParam, lParam, FALSE);
}

/******************************************************************
 *        RichEditANSIWndProc (RICHED20.10)
 */
LRESULT WINAPI RichEditANSIWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return RichEditWndProcA(hWnd, msg, wParam, lParam);
}

/******************************************************************
 *        RichEdit10ANSIWndProc (RICHED20.9)
 */
LRESULT WINAPI RichEdit10ANSIWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  if (msg == WM_NCCREATE && !GetWindowLongPtrW(hWnd, 0))
  {
    CREATESTRUCTW *pcs = (CREATESTRUCTW *)lParam;

    TRACE("WM_NCCREATE: hWnd %p style 0x%08x\n", hWnd, pcs->style);
    return create_windowed_editor( hWnd, pcs, TRUE );
  }
  return RichEditANSIWndProc(hWnd, msg, wParam, lParam);
}

void ME_SendOldNotify(ME_TextEditor *editor, int nCode)
{
  ITextHost_TxNotify(editor->texthost, nCode, NULL);
}

/* Fill buffer with srcChars unicode characters from the start cursor.
 *
 * buffer: destination buffer
 * buflen: length of buffer in characters excluding the NULL terminator.
 * start: start of editor text to copy into buffer.
 * srcChars: Number of characters to use from the editor text.
 * bCRLF: if true, replaces all end of lines with \r\n pairs.
 *
 * returns the number of characters written excluding the NULL terminator.
 *
 * The written text is always NULL terminated.
 */
int ME_GetTextW(ME_TextEditor *editor, WCHAR *buffer, int buflen,
                const ME_Cursor *start, int srcChars, BOOL bCRLF,
                BOOL bEOP)
{
  ME_Run *run, *next_run;
  const WCHAR *pStart = buffer;
  const WCHAR *str;
  int nLen;

  /* bCRLF flag is only honored in 2.0 and up. 1.0 must always return text verbatim */
  if (editor->bEmulateVersion10) bCRLF = FALSE;

  run = start->run;
  next_run = run_next_all_paras( run );

  nLen = run->len - start->nOffset;
  str = get_text( run, start->nOffset );

  while (srcChars && buflen && next_run)
  {
    if (bCRLF && run->nFlags & MERF_ENDPARA && ~run->nFlags & MERF_ENDCELL)
    {
      if (buflen == 1) break;
      /* FIXME: native fails to reduce srcChars here for WM_GETTEXT or
       *        EM_GETTEXTEX, however, this is done for copying text which
       *        also uses this function. */
      srcChars -= min(nLen, srcChars);
      nLen = 2;
      str = L"\r\n";
    }
    else
    {
      nLen = min(nLen, srcChars);
      srcChars -= nLen;
    }

    nLen = min(nLen, buflen);
    buflen -= nLen;

    CopyMemory(buffer, str, sizeof(WCHAR) * nLen);

    buffer += nLen;

    run = next_run;
    next_run = run_next_all_paras( run );

    nLen = run->len;
    str = get_text( run, 0 );
  }
  /* append '\r' to the last paragraph. */
  if (run == para_end_run( para_prev( editor_end_para( editor ) ) ) && bEOP)
  {
    *buffer = '\r';
    buffer ++;
  }
  *buffer = 0;
  return buffer - pStart;
}

static BOOL ME_RegisterEditorClass(HINSTANCE hInstance)
{
  WNDCLASSW wcW;
  WNDCLASSA wcA;
  
  wcW.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
  wcW.lpfnWndProc = RichEditWndProcW;
  wcW.cbClsExtra = 0;
  wcW.cbWndExtra = sizeof(ME_TextEditor *);
  wcW.hInstance = NULL; /* hInstance would register DLL-local class */
  wcW.hIcon = NULL;
  wcW.hCursor = LoadCursorW(NULL, (LPWSTR)IDC_IBEAM);
  wcW.hbrBackground = GetStockObject(NULL_BRUSH);
  wcW.lpszMenuName = NULL;

  if (is_version_nt())
  {
    wcW.lpszClassName = RICHEDIT_CLASS20W;
    if (!RegisterClassW(&wcW)) return FALSE;
    wcW.lpszClassName = MSFTEDIT_CLASS;
    if (!RegisterClassW(&wcW)) return FALSE;
  }
  else
  {
    /* WNDCLASSA/W have the same layout */
    wcW.lpszClassName = (LPCWSTR)"RichEdit20W";
    if (!RegisterClassA((WNDCLASSA *)&wcW)) return FALSE;
    wcW.lpszClassName = (LPCWSTR)"RichEdit50W";
    if (!RegisterClassA((WNDCLASSA *)&wcW)) return FALSE;
  }

  wcA.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
  wcA.lpfnWndProc = RichEditWndProcA;
  wcA.cbClsExtra = 0;
  wcA.cbWndExtra = sizeof(ME_TextEditor *);
  wcA.hInstance = NULL; /* hInstance would register DLL-local class */
  wcA.hIcon = NULL;
  wcA.hCursor = LoadCursorW(NULL, (LPWSTR)IDC_IBEAM);
  wcA.hbrBackground = GetStockObject(NULL_BRUSH);
  wcA.lpszMenuName = NULL;
  wcA.lpszClassName = RICHEDIT_CLASS20A;
  if (!RegisterClassA(&wcA)) return FALSE;
  wcA.lpszClassName = "RichEdit50A";
  if (!RegisterClassA(&wcA)) return FALSE;

  return TRUE;
}

static LRESULT WINAPI REComboWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  /* FIXME: Not implemented */
  TRACE("hWnd %p msg %04x (%s) %08lx %08lx\n",
        hWnd, msg, get_msg_name(msg), wParam, lParam);
  return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static LRESULT WINAPI REListWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  /* FIXME: Not implemented */
  TRACE("hWnd %p msg %04x (%s) %08lx %08lx\n",
        hWnd, msg, get_msg_name(msg), wParam, lParam);
  return DefWindowProcW(hWnd, msg, wParam, lParam);
}

/******************************************************************
 *        REExtendedRegisterClass (RICHED20.8)
 *
 * FIXME undocumented
 * Need to check for errors and implement controls and callbacks 
 */
LRESULT WINAPI REExtendedRegisterClass(void)
{
  WNDCLASSW wcW;
  UINT result;

  FIXME("semi stub\n");

  wcW.cbClsExtra = 0;
  wcW.cbWndExtra = 4;
  wcW.hInstance = NULL;
  wcW.hIcon = NULL;
  wcW.hCursor = NULL;
  wcW.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
  wcW.lpszMenuName = NULL;

  if (!ME_ListBoxRegistered)
  {
      wcW.style = CS_PARENTDC | CS_DBLCLKS | CS_GLOBALCLASS;
      wcW.lpfnWndProc = REListWndProc;
      wcW.lpszClassName = L"REListBox20W";
      if (RegisterClassW(&wcW)) ME_ListBoxRegistered = TRUE;
  }

  if (!ME_ComboBoxRegistered)
  {
      wcW.style = CS_PARENTDC | CS_DBLCLKS | CS_GLOBALCLASS | CS_VREDRAW | CS_HREDRAW;
      wcW.lpfnWndProc = REComboWndProc;
      wcW.lpszClassName = L"REComboBox20W";
      if (RegisterClassW(&wcW)) ME_ComboBoxRegistered = TRUE;  
  }

  result = 0;
  if (ME_ListBoxRegistered)
      result += 1;
  if (ME_ComboBoxRegistered)
      result += 2;

  return result;
}

static int __cdecl wchar_comp( const void *key, const void *elem )
{
    return *(const WCHAR *)key - *(const WCHAR *)elem;
}

/* neutral characters end the url if the next non-neutral character is a space character,
   otherwise they are included in the url. */
static BOOL isurlneutral( WCHAR c )
{
    /* NB this list is sorted */
    static const WCHAR neutral_chars[] = L"!\"'(),-.:;<>?[]{}";

    /* Some shortcuts */
    if (isalnum( c )) return FALSE;
    if (c > L'}') return FALSE;

    return !!bsearch( &c, neutral_chars, ARRAY_SIZE( neutral_chars ) - 1, sizeof(c), wchar_comp );
}

/**
 * This proc takes a selection, and scans it forward in order to select the span
 * of a possible URL candidate. A possible URL candidate must start with isalnum
 * or one of the following special characters: *|/\+%#@ and must consist entirely
 * of the characters allowed to start the URL, plus : (colon) which may occur
 * at most once, and not at either end.
 */
static BOOL ME_FindNextURLCandidate(ME_TextEditor *editor,
                                    const ME_Cursor *start,
                                    int nChars,
                                    ME_Cursor *candidate_min,
                                    ME_Cursor *candidate_max)
{
  ME_Cursor cursor = *start, neutral_end, space_end;
  BOOL candidateStarted = FALSE, quoted = FALSE;
  WCHAR c;

  while (nChars > 0)
  {
    WCHAR *str = get_text( cursor.run, 0 );
    int run_len = cursor.run->len;

    nChars -= run_len - cursor.nOffset;

    /* Find start of candidate */
    if (!candidateStarted)
    {
      while (cursor.nOffset < run_len)
      {
        c = str[cursor.nOffset];
        if (!iswspace( c ) && !isurlneutral( c ))
        {
          *candidate_min = cursor;
          candidateStarted = TRUE;
          neutral_end.para = NULL;
          space_end.para = NULL;
          cursor.nOffset++;
          break;
        }
        quoted = (c == '<');
        cursor.nOffset++;
      }
    }

    /* Find end of candidate */
    if (candidateStarted)
    {
      while (cursor.nOffset < run_len)
      {
        c = str[cursor.nOffset];
        if (iswspace( c ))
        {
          if (quoted && c != '\r')
          {
            if (!space_end.para)
            {
              if (neutral_end.para)
                space_end = neutral_end;
              else
                space_end = cursor;
            }
          }
          else
            goto done;
        }
        else if (isurlneutral( c ))
        {
          if (quoted && c == '>')
          {
            neutral_end.para = NULL;
            space_end.para = NULL;
            goto done;
          }
          if (!neutral_end.para)
            neutral_end = cursor;
        }
        else
          neutral_end.para = NULL;

        cursor.nOffset++;
      }
    }

    cursor.nOffset = 0;
    if (!cursor_next_run( &cursor, TRUE ))
      goto done;
  }

done:
  if (candidateStarted)
  {
    if (space_end.para)
      *candidate_max = space_end;
    else if (neutral_end.para)
      *candidate_max = neutral_end;
    else
      *candidate_max = cursor;
    return TRUE;
  }
  *candidate_max = *candidate_min = cursor;
  return FALSE;
}

/**
 * This proc evaluates the selection and returns TRUE if it can be considered an URL
 */
static BOOL ME_IsCandidateAnURL(ME_TextEditor *editor, const ME_Cursor *start, int nChars)
{
#define MAX_PREFIX_LEN 9
#define X(str)  str, ARRAY_SIZE(str) - 1
  struct prefix_s {
    const WCHAR text[MAX_PREFIX_LEN];
    int length;
  }prefixes[] = {
    {X(L"prospero:")},
    {X(L"telnet:")},
    {X(L"gopher:")},
    {X(L"mailto:")},
    {X(L"https:")},
    {X(L"file:")},
    {X(L"news:")},
    {X(L"wais:")},
    {X(L"nntp:")},
    {X(L"http:")},
    {X(L"www.")},
    {X(L"ftp:")},
  };
#undef X
  WCHAR bufferW[MAX_PREFIX_LEN + 1];
  unsigned int i;

  ME_GetTextW(editor, bufferW, MAX_PREFIX_LEN, start, nChars, FALSE, FALSE);
  for (i = 0; i < ARRAY_SIZE(prefixes); i++)
  {
    if (nChars < prefixes[i].length) continue;
    if (!memcmp(prefixes[i].text, bufferW, prefixes[i].length * sizeof(WCHAR)))
      return TRUE;
  }
  return FALSE;
#undef MAX_PREFIX_LEN
}

/**
 * This proc walks through the indicated selection and evaluates whether each
 * section identified by ME_FindNextURLCandidate and in-between sections have
 * their proper CFE_LINK attributes set or unset. If the CFE_LINK attribute is
 * not what it is supposed to be, this proc sets or unsets it as appropriate.
 *
 * Since this function can cause runs to be split, do not depend on the value
 * of the start cursor at the end of the function.
 *
 * nChars may be set to INT_MAX to update to the end of the text.
 *
 * Returns TRUE if at least one section was modified.
 */
static BOOL ME_UpdateLinkAttribute(ME_TextEditor *editor, ME_Cursor *start, int nChars)
{
  BOOL modified = FALSE;
  ME_Cursor startCur = *start;

  if (!editor->AutoURLDetect_bEnable) return FALSE;

  do
  {
    CHARFORMAT2W link;
    ME_Cursor candidateStart, candidateEnd;

    if (ME_FindNextURLCandidate(editor, &startCur, nChars,
                                &candidateStart, &candidateEnd))
    {
      /* Section before candidate is not an URL */
      int cMin = ME_GetCursorOfs(&candidateStart);
      int cMax = ME_GetCursorOfs(&candidateEnd);

      if (!ME_IsCandidateAnURL(editor, &candidateStart, cMax - cMin))
        candidateStart = candidateEnd;
      nChars -= cMax - ME_GetCursorOfs(&startCur);
    }
    else
    {
      /* No more candidates until end of selection */
      nChars = 0;
    }

    if (startCur.run != candidateStart.run ||
        startCur.nOffset != candidateStart.nOffset)
    {
      /* CFE_LINK effect should be consistently unset */
      link.cbSize = sizeof(link);
      ME_GetCharFormat(editor, &startCur, &candidateStart, &link);
      if (!(link.dwMask & CFM_LINK) || (link.dwEffects & CFE_LINK))
      {
        /* CFE_LINK must be unset from this range */
        memset(&link, 0, sizeof(CHARFORMAT2W));
        link.cbSize = sizeof(link);
        link.dwMask = CFM_LINK;
        link.dwEffects = 0;
        ME_SetCharFormat(editor, &startCur, &candidateStart, &link);
        /* Update candidateEnd since setting character formats may split
         * runs, which can cause a cursor to be at an invalid offset within
         * a split run. */
        while (candidateEnd.nOffset >= candidateEnd.run->len)
        {
          candidateEnd.nOffset -= candidateEnd.run->len;
          candidateEnd.run = run_next_all_paras( candidateEnd.run );
        }
        modified = TRUE;
      }
    }
    if (candidateStart.run != candidateEnd.run ||
        candidateStart.nOffset != candidateEnd.nOffset)
    {
      /* CFE_LINK effect should be consistently set */
      link.cbSize = sizeof(link);
      ME_GetCharFormat(editor, &candidateStart, &candidateEnd, &link);
      if (!(link.dwMask & CFM_LINK) || !(link.dwEffects & CFE_LINK))
      {
        /* CFE_LINK must be set on this range */
        memset(&link, 0, sizeof(CHARFORMAT2W));
        link.cbSize = sizeof(link);
        link.dwMask = CFM_LINK;
        link.dwEffects = CFE_LINK;
        ME_SetCharFormat(editor, &candidateStart, &candidateEnd, &link);
        modified = TRUE;
      }
    }
    startCur = candidateEnd;
  } while (nChars > 0);
  return modified;
}
