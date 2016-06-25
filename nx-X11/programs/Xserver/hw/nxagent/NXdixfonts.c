/**************************************************************************/
/*                                                                        */
/* Copyright (c) 2001, 2011 NoMachine, http://www.nomachine.com/.         */
/*                                                                        */
/* NXAGENT, NX protocol compression and NX extensions to this software    */
/* are copyright of NoMachine. Redistribution and use of the present      */
/* software is allowed according to terms specified in the file LICENSE   */
/* which comes in the source distribution.                                */
/*                                                                        */
/* Check http://www.nomachine.com/licensing.html for applicability.       */
/*                                                                        */
/* NX and NoMachine are trademarks of Medialogic S.p.A.                   */
/*                                                                        */
/* All rights reserved.                                                   */
/*                                                                        */
/**************************************************************************/

/* $XdotOrg: xc/programs/Xserver/dix/dixfonts.c,v 1.8 2005/07/03 08:53:38 daniels Exp $ */
/* $XFree86: xc/programs/Xserver/dix/dixfonts.c,v 3.28 2003/11/08 02:02:03 dawes Exp $ */
/************************************************************************
Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

************************************************************************/
/* The panoramix components contained the following notice */
/*
Copyright (c) 1991, 1997 Digital Equipment Corporation, Maynard, Massachusetts.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
DIGITAL EQUIPMENT CORPORATION BE LIABLE FOR ANY CLAIM, DAMAGES, INCLUDING,
BUT NOT LIMITED TO CONSEQUENTIAL OR INCIDENTAL DAMAGES, OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Digital Equipment Corporation
shall not be used in advertising or otherwise to promote the sale, use or other
dealings in this Software without prior written authorization from Digital
Equipment Corporation.

******************************************************************/
/* $Xorg: dixfonts.c,v 1.4 2000/08/17 19:48:18 cpqbld Exp $ */

#include "dixstruct.h"
#include "dixfontstr.h"

static Bool doOpenFont(ClientPtr client, OFclosurePtr c);
static Bool doListFontsAndAliases(ClientPtr client, LFclosurePtr c);

#include "../../dix/dixfonts.c"

/*
#define NXAGENT_DEBUG
*/

#include "Agent.h"
#include "Font.h"

#ifndef NX_TRANS_SOCKET

#define NX_TRANS_SOCKET

#endif

#ifdef NX_TRANS_SOCKET

char _NXFontPath[1024];

/*
 * Override the default font path and make
 * it configurable at run time, based on
 * the NX_FONT environment.
 */

static const char *_NXGetFontPath(const char *path)
{
  const char *fontEnv;

    /*
     * Check the environment only once.
     */

    if (*_NXFontPath != '\0')
    {
        return _NXFontPath;
    }

    fontEnv = getenv("NX_FONT");

    if (fontEnv != NULL && *fontEnv != '\0')
    {
        if (strlen(fontEnv) + 1 > 1024)
        {
#ifdef NX_TRANS_TEST
            fprintf(stderr, "_NXGetFontPath: WARNING! Maximum length of font path exceeded.\n");
#endif
            goto _NXGetFontPathError;
        }

        strcpy(_NXFontPath, fontEnv);

#ifdef NX_TRANS_TEST
        fprintf(stderr, "_NXGetFontPath: Using NX font path [%s].\n", _NXFontPath);
#endif

        return _NXFontPath;
    }

_NXGetFontPathError:

    strncpy(_NXFontPath, path, 1023);
    _NXFontPath[1023] = '\0';

#ifdef NX_TRANS_TEST
    fprintf(stderr, "_NXGetFontPath: Using default font path [%s].\n", _NXFontPath);
#endif

    return _NXFontPath;
}

#endif

static Bool
doOpenFont(ClientPtr client, OFclosurePtr c)
{
    FontPtr     pfont = NullFont;
    FontPathElementPtr fpe = NULL;
    ScreenPtr   pScr;
    int         err = Successful;
    int         i;
    char       *alias,
               *newname;
    int         newlen;
    int		aliascount = 20;
    char nxagentOrigFontName[256];
    int nxagentOrigFontNameLen;

    /*
     * Decide at runtime what FontFormat to use.
     */
    Mask FontFormat = 

	((screenInfo.imageByteOrder == LSBFirst) ?
	    BitmapFormatByteOrderLSB : BitmapFormatByteOrderMSB) |

	((screenInfo.bitmapBitOrder == LSBFirst) ?
	    BitmapFormatBitOrderLSB : BitmapFormatBitOrderMSB) |

	BitmapFormatImageRectMin |

#if GLYPHPADBYTES == 1
	BitmapFormatScanlinePad8 |
#endif

#if GLYPHPADBYTES == 2
	BitmapFormatScanlinePad16 |
#endif

#if GLYPHPADBYTES == 4
	BitmapFormatScanlinePad32 |
#endif

#if GLYPHPADBYTES == 8
	BitmapFormatScanlinePad64 |
#endif

	BitmapFormatScanlineUnit8;


    nxagentOrigFontNameLen = (c -> origFontNameLen < 256) ? c -> origFontNameLen : 255;

    memcpy(nxagentOrigFontName, c -> origFontName, nxagentOrigFontNameLen);

    nxagentOrigFontName[nxagentOrigFontNameLen] = 0;

    if (client->clientGone)
    {
	if (c->current_fpe < c->num_fpes)
	{
	    fpe = c->fpe_list[c->current_fpe];
	    (*fpe_functions[fpe->type].client_died) ((void *) client, fpe);
	}
	err = Successful;
	goto bail;
    }
    while (c->current_fpe < c->num_fpes) {
	fpe = c->fpe_list[c->current_fpe];
	err = (*fpe_functions[fpe->type].open_font)
	    ((void *) client, fpe, c->flags,
	     c->fontname, c->fnamelen, FontFormat,
	     BitmapFormatMaskByte |
	     BitmapFormatMaskBit |
	     BitmapFormatMaskImageRectangle |
	     BitmapFormatMaskScanLinePad |
	     BitmapFormatMaskScanLineUnit,
	     c->fontid, &pfont, &alias,
	     c->non_cachable_font && c->non_cachable_font->fpe == fpe ?
		 c->non_cachable_font :
		 (FontPtr)0);

	if (err == FontNameAlias && alias) {
	    newlen = strlen(alias);
	    newname = (char *) xrealloc(c->fontname, newlen);
	    if (!newname) {
		err = AllocError;
		break;
	    }
	    memmove(newname, alias, newlen);
	    c->fontname = newname;
	    c->fnamelen = newlen;
	    c->current_fpe = 0;
	    if (--aliascount <= 0)
		break;
	    continue;
	}
	if (err == BadFontName) {
	    c->current_fpe++;
	    continue;
	}
	if (err == Suspended) {
	    if (!c->slept) {
		c->slept = TRUE;
		ClientSleep(client, (ClientSleepProcPtr)doOpenFont, (void *) c);
#ifdef NXAGENT_DEBUG
                fprintf(stderr, " NXdixfonts: doOpenFont: client [%lx] sleeping.\n", client);
#endif
	    }
	    return TRUE;
	}
	break;
    }

    if (err != Successful)
	goto bail;
    if (!pfont) {
	err = BadFontName;
	goto bail;
    }
    /* check values for firstCol, lastCol, firstRow, and lastRow */
    if (pfont->info.firstCol > pfont->info.lastCol ||
       pfont->info.firstRow > pfont->info.lastRow ||
       pfont->info.lastCol - pfont->info.firstCol > 255) {
       err = AllocError;
       goto bail;
    }
    if (!pfont->fpe)
	pfont->fpe = fpe;
    pfont->refcnt++;
    if (pfont->refcnt == 1) {
	UseFPE(pfont->fpe);
	for (i = 0; i < screenInfo.numScreens; i++) {
	    pScr = screenInfo.screens[i];
	    if (pScr->RealizeFont)
	    {

                /* NXAGENT uses useless screen pointer to pass the original font name
                *  to realizeFont, could be a source of problems in the future.
                */

		if (!(*pScr->RealizeFont) ((ScreenPtr)nxagentOrigFontName, pfont))
		{
		    CloseFont (pfont, (Font) 0);
		    err=BadFontName;
		    goto bail;
		}
	    }
	}
    }
    if (!AddResource(c->fontid, RT_FONT, (void *) pfont)) {
	err = AllocError;
	goto bail;
    }
    if( nxagentFontPriv(pfont) -> mirrorID == 0 )
    {
      extern RESTYPE RT_NX_FONT;

      nxagentFontPriv(pfont) -> mirrorID = FakeClientID(0);
      if (!AddResource(nxagentFontPriv(pfont) -> mirrorID, RT_NX_FONT, (void *) pfont)) {
        FreeResource(c->fontid, RT_NONE);
        err = AllocError;
        goto bail;
      }
    }
    if (patternCache && pfont != c->non_cachable_font)
	CacheFontPattern(patternCache, nxagentOrigFontName, nxagentOrigFontNameLen,
			 pfont);
bail:
    if (err != Successful && c->client != serverClient) {
	SendErrorToClient(c->client, X_OpenFont, 0,
			  c->fontid, FontToXError(err));
    }
    if (c->slept)
    {
	ClientWakeup(c->client);
#ifdef NXAGENT_DEBUG
        fprintf(stderr, " NXdixfonts: doOpenFont: client [%lx] wakeup.\n", client);
#endif
    }
    for (i = 0; i < c->num_fpes; i++) {
	FreeFPE(c->fpe_list[i]);
    }
    free(c->fpe_list);
    free(c->fontname);
    free(c);
    return TRUE;
}


static Bool
doListFontsAndAliases(ClientPtr client, LFclosurePtr c)
{
    FontPathElementPtr fpe;
    int         err = Successful;
    FontNamesPtr names = NULL;
    char       *name, *resolved=NULL;
    int         namelen, resolvedlen;
    int		nnames;
    int         stringLens;
    int         i;
    xListFontsReply reply;
    char	*bufptr;
    char	*bufferStart;
    int		aliascount = 0;

    if (client->clientGone)
    {
	if (c->current.current_fpe < c->num_fpes)
	{
	    fpe = c->fpe_list[c->current.current_fpe];
	    (*fpe_functions[fpe->type].client_died) ((void *) client, fpe);
	}
	err = Successful;
	goto bail;
    }

    if (!c->current.patlen)
	goto finish;

    while (c->current.current_fpe < c->num_fpes) {
	fpe = c->fpe_list[c->current.current_fpe];
	err = Successful;

	if (!fpe_functions[fpe->type].start_list_fonts_and_aliases)
	{
	    /* This FPE doesn't support/require list_fonts_and_aliases */

	    err = (*fpe_functions[fpe->type].list_fonts)
		((void *) c->client, fpe, c->current.pattern,
		 c->current.patlen, c->current.max_names - c->names->nnames,
		 c->names);

	    if (err == Suspended) {
		if (!c->slept) {
		    c->slept = TRUE;
		    ClientSleep(client,
			(ClientSleepProcPtr)doListFontsAndAliases,
			(void *) c);
#ifdef NXAGENT_DEBUG
                    fprintf(stderr, " NXdixfonts: doListFont (1): client [%lx] sleeping.\n", client);
#endif
		}
		return TRUE;
	    }

	    err = BadFontName;
	}
	else
	{
	    /* Start of list_fonts_and_aliases functionality.  Modeled
	       after list_fonts_with_info in that it resolves aliases,
	       except that the information collected from FPEs is just
	       names, not font info.  Each list_next_font_or_alias()
	       returns either a name into name/namelen or an alias into
	       name/namelen and its target name into resolved/resolvedlen.
	       The code at this level then resolves the alias by polling
	       the FPEs.  */

	    if (!c->current.list_started) {
		err = (*fpe_functions[fpe->type].start_list_fonts_and_aliases)
		    ((void *) c->client, fpe, c->current.pattern,
		     c->current.patlen, c->current.max_names - c->names->nnames,
		     &c->current.private);
		if (err == Suspended) {
		    if (!c->slept) {
			ClientSleep(client,
				    (ClientSleepProcPtr)doListFontsAndAliases,
				    (void *) c);
			c->slept = TRUE;
		    }
		    return TRUE;
		}
		if (err == Successful)
		    c->current.list_started = TRUE;
	    }
	    if (err == Successful) {
		char    *tmpname;
		name = 0;
		err = (*fpe_functions[fpe->type].list_next_font_or_alias)
		    ((void *) c->client, fpe, &name, &namelen, &tmpname,
		     &resolvedlen, c->current.private);
		if (err == Suspended) {
		    if (!c->slept) {
			ClientSleep(client,
				    (ClientSleepProcPtr)doListFontsAndAliases,
				    (void *) c);
			c->slept = TRUE;
#ifdef NXAGENT_DEBUG
                        fprintf(stderr, " NXdixfonts: doListFont (2): client [%lx] sleeping.\n", client);
#endif
#ifdef NXAGENT_DEBUG
                        fprintf(stderr, " NXdixfonts: doListFont (3): client [%lx] sleeping.\n", client);
#endif
		    }
		    return TRUE;
		}
		if (err == FontNameAlias) {
		    if (resolved) free(resolved);
		    resolved = (char *) xalloc(resolvedlen + 1);
		    if (resolved)
			memmove(resolved, tmpname, resolvedlen + 1);
		}
	    }

	    if (err == Successful)
	    {
		if (c->haveSaved)
		{
		    if (c->savedName)
			(void)AddFontNamesName(c->names, c->savedName,
					       c->savedNameLen);
		}
		else
		    (void)AddFontNamesName(c->names, name, namelen);
	    }

	    /*
	     * When we get an alias back, save our state and reset back to
	     * the start of the FPE looking for the specified name.  As
	     * soon as a real font is found for the alias, pop back to the
	     * old state
	     */
	    else if (err == FontNameAlias) {
		char	tmp_pattern[XLFDMAXFONTNAMELEN];
		/*
		 * when an alias recurses, we need to give
		 * the last FPE a chance to clean up; so we call
		 * it again, and assume that the error returned
		 * is BadFontName, indicating the alias resolution
		 * is complete.
		 */
		memmove(tmp_pattern, resolved, resolvedlen);
		if (c->haveSaved)
		{
		    char    *tmpname;
		    int     tmpnamelen;

		    tmpname = 0;
		    (void) (*fpe_functions[fpe->type].list_next_font_or_alias)
			((void *) c->client, fpe, &tmpname, &tmpnamelen,
			 &tmpname, &tmpnamelen, c->current.private);
		    if (--aliascount <= 0)
		    {
			err = BadFontName;
			goto ContBadFontName;
		    }
		}
		else
		{
		    c->saved = c->current;
		    c->haveSaved = TRUE;
		    if (c->savedName)
			free(c->savedName);
		    c->savedName = (char *)xalloc(namelen + 1);
		    if (c->savedName)
			memmove(c->savedName, name, namelen + 1);
		    c->savedNameLen = namelen;
		    aliascount = 20;
		}
		memmove(c->current.pattern, tmp_pattern, resolvedlen);
		c->current.patlen = resolvedlen;
		c->current.max_names = c->names->nnames + 1;
		c->current.current_fpe = -1;
		c->current.private = 0;
		err = BadFontName;
	    }
	}
	/*
	 * At the end of this FPE, step to the next.  If we've finished
	 * processing an alias, pop state back. If we've collected enough
	 * font names, quit.
	 */
	if (err == BadFontName) {
	  ContBadFontName: ;
	    c->current.list_started = FALSE;
	    c->current.current_fpe++;
	    err = Successful;
	    if (c->haveSaved)
	    {
		if (c->names->nnames == c->current.max_names ||
			c->current.current_fpe == c->num_fpes) {
		    c->haveSaved = FALSE;
		    c->current = c->saved;
		    /* Give the saved namelist a chance to clean itself up */
		    continue;
		}
	    }
	    if (c->names->nnames == c->current.max_names)
		break;
	}
    }

    /*
     * send the reply
     */
    if (err != Successful) {
	SendErrorToClient(client, X_ListFonts, 0, 0, FontToXError(err));
	goto bail;
    }

finish:

    names = c->names;
    nnames = names->nnames;
    client = c->client;
    stringLens = 0;
    for (i = 0; i < nnames; i++)
	stringLens += (names->length[i] <= 255) ? names->length[i] : 0;

    memset(&reply, 0, sizeof(xListFontsReply));
    reply.type = X_Reply;
    reply.length = (stringLens + nnames + 3) >> 2;
    reply.nFonts = nnames;
    reply.sequenceNumber = client->sequence;

    bufptr = bufferStart = (char *) ALLOCATE_LOCAL(reply.length << 2);

    if (!bufptr && reply.length) {
	SendErrorToClient(client, X_ListFonts, 0, 0, BadAlloc);
	goto bail;
    }
    /*
     * since WriteToClient long word aligns things, copy to temp buffer and
     * write all at once
     */
    for (i = 0; i < nnames; i++) {
	if (names->length[i] > 255)
	    reply.nFonts--;
	else
	{
	    {
	      /* dirty hack: don't list to client fonts not existing on the remote side */
	      char tmp[256];

	      memcpy(tmp, names->names[i], names->length[i]);
	      tmp[ names->length[i] ] = 0;

	      if (nxagentFontLookUp(tmp) == 0)
		{
#ifdef NXAGENT_FONTMATCH_DEBUG
		  fprintf(stderr, "doListFontsAndAliases:\n");
		  fprintf(stderr, "      removing font: %s \n", tmp);
#endif
		  reply.nFonts--;
		  stringLens -= names->length[i];
		  continue;
		}
	    }
	    *bufptr++ = names->length[i];
	    memmove( bufptr, names->names[i], names->length[i]);
	    bufptr += names->length[i];
	}
    }
    nnames = reply.nFonts;
    reply.length = (stringLens + nnames + 3) >> 2;
    client->pSwapReplyFunc = ReplySwapVector[X_ListFonts];
    WriteSwappedDataToClient(client, sizeof(xListFontsReply), &reply);
    (void) WriteToClient(client, stringLens + nnames, bufferStart);
    DEALLOCATE_LOCAL(bufferStart);

bail:
    if (c->slept)
    {
	ClientWakeup(client);
#ifdef NXAGENT_DEBUG
        fprintf(stderr, " NXdixfonts: doListFont: client [%lx] wakeup.\n", client);
#endif
    }
    for (i = 0; i < c->num_fpes; i++)
	FreeFPE(c->fpe_list[i]);
    free(c->fpe_list);
    if (c->savedName) free(c->savedName);
    FreeFontNames(names);
    free(c);
    if (resolved) free(resolved);
    return TRUE;
}

int
ListFonts(ClientPtr client, unsigned char *pattern, unsigned length, 
          unsigned max_names)
{
    int         i;
    LFclosurePtr c;

    /* 
     * The right error to return here would be BadName, however the
     * specification does not allow for a Name error on this request.
     * Perhaps a better solution would be to return a nil list, i.e.
     * a list containing zero fontnames.
     */
    if (length > XLFDMAXFONTNAMELEN)
	return BadAlloc;

    if (!(c = (LFclosurePtr) xalloc(sizeof *c)))
	return BadAlloc;
    c->fpe_list = (FontPathElementPtr *)
	xalloc(sizeof(FontPathElementPtr) * num_fpes);
    if (!c->fpe_list) {
	free(c);
	return BadAlloc;
    }
    c->names = MakeFontNamesRecord(max_names < nxagentMaxFontNames ? max_names : nxagentMaxFontNames);
    if (!c->names)
    {
	free(c->fpe_list);
	free(c);
	return BadAlloc;
    }
    memmove( c->current.pattern, pattern, length);
    for (i = 0; i < num_fpes; i++) {
	c->fpe_list[i] = font_path_elements[i];
	UseFPE(c->fpe_list[i]);
    }
    c->client = client;
    c->num_fpes = num_fpes;
    c->current.patlen = length;
    c->current.current_fpe = 0;
    c->current.max_names = max_names;
    c->current.list_started = FALSE;
    c->current.private = 0;
    c->haveSaved = FALSE;
    c->slept = FALSE;
    c->savedName = 0;
    doListFontsAndAliases(client, c);
    return Success;
}

int
doListFontsWithInfo(ClientPtr client, LFWIclosurePtr c)
{
    FontPathElementPtr fpe;
    int         err = Successful;
    char       *name;
    int         namelen;
    int         numFonts;
    FontInfoRec fontInfo,
               *pFontInfo;
    xListFontsWithInfoReply *reply;
    int         length;
    xFontProp  *pFP;
    int         i;
    int		aliascount = 0;
    xListFontsWithInfoReply finalReply;

    if (client->clientGone)
    {
	if (c->current.current_fpe < c->num_fpes)
 	{
	    fpe = c->fpe_list[c->current.current_fpe];
	    (*fpe_functions[fpe->type].client_died) ((void *) client, fpe);
	}
	err = Successful;
	goto bail;
    }
    client->pSwapReplyFunc = ReplySwapVector[X_ListFontsWithInfo];
    if (!c->current.patlen)
	goto finish;
    while (c->current.current_fpe < c->num_fpes)
    {
	fpe = c->fpe_list[c->current.current_fpe];
	err = Successful;
	if (!c->current.list_started)
 	{
	    err = (*fpe_functions[fpe->type].start_list_fonts_with_info)
		(client, fpe, c->current.pattern, c->current.patlen,
		 c->current.max_names, &c->current.private);
	    if (err == Suspended)
 	    {
		if (!c->slept)
 		{
		    ClientSleep(client, (ClientSleepProcPtr)doListFontsWithInfo, c);
		    c->slept = TRUE;
#ifdef NXAGENT_DEBUG
                    fprintf(stderr, " NXdixfonts: doListFontWinfo (1): client [%lx] sleeping.\n", client);
#endif
		}
		return TRUE;
	    }
	    if (err == Successful)
		c->current.list_started = TRUE;
	}
	if (err == Successful)
 	{
	    name = 0;
	    pFontInfo = &fontInfo;
	    err = (*fpe_functions[fpe->type].list_next_font_with_info)
		(client, fpe, &name, &namelen, &pFontInfo,
		 &numFonts, c->current.private);
	    if (err == Suspended)
 	    {
		if (!c->slept)
 		{
		    ClientSleep(client,
		    	     (ClientSleepProcPtr)doListFontsWithInfo,
			     c);
		    c->slept = TRUE;
#ifdef NXAGENT_DEBUG
                    fprintf(stderr, " NXdixfonts: doListFontWinfo (2): client [%lx] sleeping.\n", client);
#endif
		}
		return TRUE;
	    }
	}
	/*
	 * When we get an alias back, save our state and reset back to the
	 * start of the FPE looking for the specified name.  As soon as a real
	 * font is found for the alias, pop back to the old state
	 */
	if (err == FontNameAlias)
 	{
	    /*
	     * when an alias recurses, we need to give
	     * the last FPE a chance to clean up; so we call
	     * it again, and assume that the error returned
	     * is BadFontName, indicating the alias resolution
	     * is complete.
	     */
	    if (c->haveSaved)
	    {
		char	*tmpname;
		int	tmpnamelen;
		FontInfoPtr tmpFontInfo;

	    	tmpname = 0;
	    	tmpFontInfo = &fontInfo;
	    	(void) (*fpe_functions[fpe->type].list_next_font_with_info)
		    (client, fpe, &tmpname, &tmpnamelen, &tmpFontInfo,
		     &numFonts, c->current.private);
		if (--aliascount <= 0)
		{
		    err = BadFontName;
		    goto ContBadFontName;
		}
	    }
	    else
	    {
		c->saved = c->current;
		c->haveSaved = TRUE;
		c->savedNumFonts = numFonts;
		if (c->savedName)
		  free(c->savedName);
		c->savedName = (char *)xalloc(namelen + 1);
		if (c->savedName)
		  memmove(c->savedName, name, namelen + 1);
		aliascount = 20;
	    }
	    memmove(c->current.pattern, name, namelen);
	    c->current.patlen = namelen;
	    c->current.max_names = 1;
	    c->current.current_fpe = 0;
	    c->current.private = 0;
	    c->current.list_started = FALSE;
	}
	/*
	 * At the end of this FPE, step to the next.  If we've finished
	 * processing an alias, pop state back.  If we've sent enough font
	 * names, quit.  Always wait for BadFontName to let the FPE
	 * have a chance to clean up.
	 */
	else if (err == BadFontName)
 	{
	  ContBadFontName: ;
	    c->current.list_started = FALSE;
	    c->current.current_fpe++;
	    err = Successful;
	    if (c->haveSaved)
 	    {
		if (c->current.max_names == 0 ||
			c->current.current_fpe == c->num_fpes)
 		{
		    c->haveSaved = FALSE;
		    c->saved.max_names -= (1 - c->current.max_names);
		    c->current = c->saved;
		}
	    }
	    else if (c->current.max_names == 0)
		break;
	}
 	else if (err == Successful)
 	{

	    if (c->haveSaved)
 	    {
		numFonts = c->savedNumFonts;
		name = c->savedName;
		namelen = strlen(name);
	    }

	   if (nxagentFontLookUp(name) == 0)
	   {
#ifdef NXAGENT_FONTMATCH_DEBUG
	      fprintf(stderr, "doListFontsAndAliases (with info):\n");
	      fprintf(stderr, "      removing font: %s \n", name);
#endif
	       continue;
           }

	    length = sizeof(*reply) + pFontInfo->nprops * sizeof(xFontProp);
	    reply = c->reply;
	    if (c->length < length)
 	    {
		reply = (xListFontsWithInfoReply *) xrealloc(c->reply, length);
		if (!reply)
 		{
		    err = AllocError;
		    break;
		}
		memset(reply + c->length, 0, length - c->length);
		c->reply = reply;
		c->length = length;
	    }
	    reply->type = X_Reply;
	    reply->length = (sizeof *reply - sizeof(xGenericReply) +
			     pFontInfo->nprops * sizeof(xFontProp) +
			     namelen + 3) >> 2;
	    reply->sequenceNumber = client->sequence;
	    reply->nameLength = namelen;
	    reply->minBounds = pFontInfo->ink_minbounds;
	    reply->maxBounds = pFontInfo->ink_maxbounds;
	    reply->minCharOrByte2 = pFontInfo->firstCol;
	    reply->maxCharOrByte2 = pFontInfo->lastCol;
	    reply->defaultChar = pFontInfo->defaultCh;
	    reply->nFontProps = pFontInfo->nprops;
	    reply->drawDirection = pFontInfo->drawDirection;
	    reply->minByte1 = pFontInfo->firstRow;
	    reply->maxByte1 = pFontInfo->lastRow;
	    reply->allCharsExist = pFontInfo->allExist;
	    reply->fontAscent = pFontInfo->fontAscent;
	    reply->fontDescent = pFontInfo->fontDescent;
	    reply->nReplies = numFonts;
	    pFP = (xFontProp *) (reply + 1);
	    for (i = 0; i < pFontInfo->nprops; i++)
 	    {
		pFP->name = pFontInfo->props[i].name;
		pFP->value = pFontInfo->props[i].value;
		pFP++;
	    }
	    WriteSwappedDataToClient(client, length, reply);
	    (void) WriteToClient(client, namelen, name);
	    if (pFontInfo == &fontInfo)
 	    {
		free(fontInfo.props);
		free(fontInfo.isStringProp);
	    }
	    --c->current.max_names;
	}
    }
finish:
    length = sizeof(xListFontsWithInfoReply);
    bzero((char *) &finalReply, sizeof(xListFontsWithInfoReply));
    finalReply.type = X_Reply;
    finalReply.sequenceNumber = client->sequence;
    finalReply.length = (sizeof(xListFontsWithInfoReply)
		     - sizeof(xGenericReply)) >> 2;
    WriteSwappedDataToClient(client, length, &finalReply);
bail:
    if (c->slept)
    {
	ClientWakeup(client);
#ifdef NXAGENT_DEBUG
        fprintf(stderr, " NXdixfonts: doListFontWinfo: client [%lx] wakeup.\n", client);
#endif
    }
    for (i = 0; i < c->num_fpes; i++)
	FreeFPE(c->fpe_list[i]);
    free(c->reply);
    free(c->fpe_list);
    if (c->savedName) free(c->savedName);
    free(c);
    return TRUE;
}


int
SetDefaultFontPath(char *path)
{
    unsigned char *cp,
               *pp,
               *nump,
               *newpath;
    int         num = 1,
                len,
                err,
                size = 0,
                bad;

    /* get enough for string, plus values -- use up commas */
#ifdef NX_TRANS_SOCKET
    len = strlen(_NXGetFontPath(path)) + 1;
#else
    len = strlen(path) + 1;
#endif
    nump = cp = newpath = (unsigned char *) ALLOCATE_LOCAL(len);
    if (!newpath)
	return BadAlloc;
#ifdef NX_TRANS_SOCKET
    pp = (unsigned char *) _NXGetFontPath(path);
#else
    pp = (unsigned char *) path;
#endif
    cp++;
    while (*pp) {
	if (*pp == ',') {
	    *nump = (unsigned char) size;
	    nump = cp++;
	    pp++;
	    num++;
	    size = 0;
	} else {
	    *cp++ = *pp++;
	    size++;
	}
    }
    *nump = (unsigned char) size;

    err = SetFontPathElements(num, newpath, &bad, TRUE);

    DEALLOCATE_LOCAL(newpath);

    return err;
}


typedef struct
{
   LFclosurePtr c;
   OFclosurePtr oc;
} nxFs,*nxFsPtr;

static Bool
#if NeedFunctionPrototypes
nxdoListFontsAndAliases(ClientPtr client, nxFsPtr fss)
#else
nxdoListFontsAndAliases(client, fss)
    ClientPtr   client;
    nxFsPtr fss;
#endif
{
    LFclosurePtr c=fss->c;
    OFclosurePtr oc=fss->oc;
    FontPathElementPtr fpe;
    int         err = Successful;
    char       *name, *resolved=NULL;
    int         namelen, resolvedlen;
    int         i;
    int		aliascount = 0;
    char        tmp[256];
    tmp[0]=0;
    if (client->clientGone)
    {
	if (c->current.current_fpe < c->num_fpes)
	{
	    fpe = c->fpe_list[c->current.current_fpe];
	    (*fpe_functions[fpe->type].client_died) ((void *) client, fpe);
	}
	err = Successful;
	goto bail;
    }

    if (!c->current.patlen)
	goto finish;

    while (c->current.current_fpe < c->num_fpes) {
	fpe = c->fpe_list[c->current.current_fpe];
	err = Successful;

	if (!fpe_functions[fpe->type].start_list_fonts_and_aliases)
	{
	    /* This FPE doesn't support/require list_fonts_and_aliases */

	    err = (*fpe_functions[fpe->type].list_fonts)
		((void *) c->client, fpe, c->current.pattern,
		 c->current.patlen, c->current.max_names - c->names->nnames,
		 c->names);

	    if (err == Suspended) {
		if (!c->slept) {
		    c->slept = TRUE;
		    ClientSleep(client,
			(ClientSleepProcPtr)nxdoListFontsAndAliases,
			(void *) fss);
#ifdef NXAGENT_DEBUG
                    fprintf(stderr, " NXdixfonts: nxdoListFont (1): client [%lx] sleeping.\n", client);
#endif
		}
		return TRUE;
	    }

	    err = BadFontName;
	}
	else
	{
	    /* Start of list_fonts_and_aliases functionality.  Modeled
	       after list_fonts_with_info in that it resolves aliases,
	       except that the information collected from FPEs is just
	       names, not font info.  Each list_next_font_or_alias()
	       returns either a name into name/namelen or an alias into
	       name/namelen and its target name into resolved/resolvedlen.
	       The code at this level then resolves the alias by polling
	       the FPEs.  */

	    if (!c->current.list_started) {
		err = (*fpe_functions[fpe->type].start_list_fonts_and_aliases)
		    ((void *) c->client, fpe, c->current.pattern,
		     c->current.patlen, c->current.max_names - c->names->nnames,
		     &c->current.private);
		if (err == Suspended) {
		    if (!c->slept) {
			ClientSleep(client,
				    (ClientSleepProcPtr)nxdoListFontsAndAliases,
				    (void *) fss);
			c->slept = TRUE;
#ifdef NXAGENT_DEBUG
                        fprintf(stderr, " NXdixfonts: nxdoListFont (2): client [%lx] sleeping.\n", client);
#endif
		    }
		    return TRUE;
		}
		if (err == Successful)
		    c->current.list_started = TRUE;
	    }
	    if (err == Successful) {
		char    *tmpname;
		name = 0;
		err = (*fpe_functions[fpe->type].list_next_font_or_alias)
		    ((void *) c->client, fpe, &name, &namelen, &tmpname,
		     &resolvedlen, c->current.private);
		if (err == Suspended) {
		    if (!c->slept) {
			ClientSleep(client,
				    (ClientSleepProcPtr)nxdoListFontsAndAliases,
				    (void *) fss);
			c->slept = TRUE;
#ifdef NXAGENT_DEBUG
                        fprintf(stderr, " NXdixfonts: nxdoListFont (3): client [%lx] sleeping.\n", client);
#endif
		    }
		    return TRUE;
		}
		if (err == FontNameAlias) {
		    if (resolved) free(resolved);
		    resolved = (char *) xalloc(resolvedlen + 1);
		    if (resolved)
                    {
                        memmove(resolved, tmpname, resolvedlen);
                        resolved[resolvedlen] = '\0';
                    }
		}
	    }

	    if (err == Successful)
	    {
		if (c->haveSaved)
		{
		    if (c->savedName)
		    {
		       memcpy(tmp,c->savedName,c->savedNameLen>255?255:c->savedNameLen);
		       tmp[c->savedNameLen>255?256:c->savedNameLen]=0;
		       if (nxagentFontLookUp(tmp))
		          break;
			else tmp[0]=0;
		    }
		}
		else
		{
		   memcpy(tmp,name,namelen>255?255:namelen);
		   tmp[namelen>255?256:namelen]=0;
		   if (nxagentFontLookUp(tmp))
		      break;
		   else tmp[0]=0;
		}
	    }

	    /*
	     * When we get an alias back, save our state and reset back to
	     * the start of the FPE looking for the specified name.  As
	     * soon as a real font is found for the alias, pop back to the
	     * old state
	     */
	    else if (err == FontNameAlias) {
		char	tmp_pattern[XLFDMAXFONTNAMELEN];
		/*
		 * when an alias recurses, we need to give
		 * the last FPE a chance to clean up; so we call
		 * it again, and assume that the error returned
		 * is BadFontName, indicating the alias resolution
		 * is complete.
		 */
		memmove(tmp_pattern, resolved, resolvedlen);
		if (c->haveSaved)
		{
		    char    *tmpname;
		    int     tmpnamelen;

		    tmpname = 0;
		    (void) (*fpe_functions[fpe->type].list_next_font_or_alias)
			((void *) c->client, fpe, &tmpname, &tmpnamelen,
			 &tmpname, &tmpnamelen, c->current.private);
		    if (--aliascount <= 0)
		    {
			err = BadFontName;
			goto ContBadFontName;
		    }
		}
		else
		{
		    c->saved = c->current;
		    c->haveSaved = TRUE;
		    if (c->savedName)
			free(c->savedName);
		    c->savedName = (char *)xalloc(namelen + 1);
		    if (c->savedName)
                    {
                        memmove(c->savedName, name, namelen);
                        c->savedName[namelen] = '\0';
                    }
		    c->savedNameLen = namelen;
		    aliascount = 20;
		}
		memmove(c->current.pattern, tmp_pattern, resolvedlen);
		c->current.patlen = resolvedlen;
		c->current.max_names = c->names->nnames + 1;
		c->current.current_fpe = -1;
		c->current.private = 0;
		err = BadFontName;
	    }
	}
	/*
	 * At the end of this FPE, step to the next.  If we've finished
	 * processing an alias, pop state back. If we've collected enough
	 * font names, quit.
	 */
	if (err == BadFontName) {
	  ContBadFontName: ;
	    c->current.list_started = FALSE;
	    c->current.current_fpe++;
	    err = Successful;
	    if (c->haveSaved)
	    {
		if (c->names->nnames == c->current.max_names ||
			c->current.current_fpe == c->num_fpes) {
		    c->haveSaved = FALSE;
		    c->current = c->saved;
		    /* Give the saved namelist a chance to clean itself up */
		    continue;
		}
	    }
	    if (c->names->nnames == c->current.max_names)
		break;
	}
    }

    /*
     * send the reply
     */
bail:
finish:
    if (strlen(tmp))
    {
#ifdef NXAGENT_FONTMATCH_DEBUG
      fprintf(stderr, "nxListFont changed (0) font to %s\n",tmp);
#endif
      memcpy(oc->fontname, tmp, strlen(tmp));
      oc->fnamelen = strlen(tmp);

      oc->origFontName = oc->fontname;
      oc->origFontNameLen = oc->fnamelen;

    }
    else
    {
        for (i = 0; i < c->names->nnames; i++)
	{
	  if (c->names->length[i] > 255)
	     continue;
	  else
	  {
	      memcpy(tmp, c->names->names[i], c->names->length[i]);
	      tmp[ c->names->length[i] ] = 0;
	      if (nxagentFontLookUp(tmp) == 0)
		continue;
	      memcpy(oc->fontname, tmp, strlen(tmp));
	      oc->fnamelen = strlen(tmp);

              oc->origFontName = oc->fontname;
              oc->origFontNameLen = oc->fnamelen;

#ifdef NXAGENT_FONTMATCH_DEBUG
	      fprintf(stderr, "nxListFont changed (1) font to %s\n",tmp);
#endif
	      break;
	  }
	}
    }

    if (c->slept)
    {
       ClientWakeup(client);
#ifdef NXAGENT_DEBUG
       fprintf(stderr, " NXdixfonts: nxdoListFont: client [%lx] wakeup.\n", client);
#endif
    }
    for (i = 0; i < c->num_fpes; i++)
	FreeFPE(c->fpe_list[i]);
    free(c->fpe_list);
    if (c->savedName) free(c->savedName);
    FreeFontNames(c->names);
    free(c);
    free(fss);
    if (resolved) free(resolved);

    return doOpenFont(client, oc);
}

int
nxOpenFont(client, fid, flags, lenfname, pfontname)
    ClientPtr   client;
    XID         fid;
    Mask        flags;
    unsigned    lenfname;
    char       *pfontname;
{
    nxFsPtr      fss;
    LFclosurePtr c;
    OFclosurePtr oc;
    int         i;
    FontPtr     cached = (FontPtr)0;

#ifdef FONTDEBUG
    char *f;
    f = (char *)xalloc(lenfname + 1);
    memmove(f, pfontname, lenfname);
    f[lenfname] = '\0';
    ErrorF("OpenFont: fontname is \"%s\"\n", f);
    free(f);
#endif
    if (!lenfname || lenfname > XLFDMAXFONTNAMELEN)
	return BadName;
    if (patternCache)
    {

    /*
    ** Check name cache.  If we find a cached version of this font that
    ** is cachable, immediately satisfy the request with it.  If we find
    ** a cached version of this font that is non-cachable, we do not
    ** satisfy the request with it.  Instead, we pass the FontPtr to the
    ** FPE's open_font code (the fontfile FPE in turn passes the
    ** information to the rasterizer; the fserve FPE ignores it).
    **
    ** Presumably, the font is marked non-cachable because the FPE has
    ** put some licensing restrictions on it.  If the FPE, using
    ** whatever logic it relies on, determines that it is willing to
    ** share this existing font with the client, then it has the option
    ** to return the FontPtr we passed it as the newly-opened font.
    ** This allows the FPE to exercise its licensing logic without
    ** having to create another instance of a font that already exists.
    */

	cached = FindCachedFontPattern(patternCache, pfontname, lenfname);
	if (cached && cached->info.cachable)
	{
	    if (!AddResource(fid, RT_FONT, (void *) cached))
		return BadAlloc;
	    cached->refcnt++;
	    return Success;
	}
    }
    if (!(fss = (nxFsPtr) xalloc(sizeof(nxFs))))
        return BadAlloc;

    if (!(c = (LFclosurePtr) xalloc(sizeof *c)))
    {
	free(fss);
	return BadAlloc;
    }
        c->fpe_list = (FontPathElementPtr *)
	xalloc(sizeof(FontPathElementPtr) * num_fpes);
    if (!c->fpe_list) {
	free(c);
	free(fss);
	return BadAlloc;
    }
    c->names = MakeFontNamesRecord(100);
    if (!c->names)
    {
	free(c->fpe_list);
	free(c);
	free(fss);
	return BadAlloc;
    }
    memmove( c->current.pattern, pfontname, lenfname);
    for (i = 0; i < num_fpes; i++) {
	c->fpe_list[i] = font_path_elements[i];
	UseFPE(c->fpe_list[i]);
    }
    c->client = client;
    c->num_fpes = num_fpes;
    c->current.patlen = lenfname;
    c->current.current_fpe = 0;
    c->current.max_names = nxagentMaxFontNames;
    c->current.list_started = FALSE;
    c->current.private = 0;
    c->haveSaved = FALSE;
    c->slept = FALSE;
    c->savedName = 0;

    oc = (OFclosurePtr) xalloc(sizeof(OFclosureRec));
    if (!oc)
    {
      for (i = 0; i < c->num_fpes; i++)
        FreeFPE(c->fpe_list[i]);
      free(c->fpe_list);
      free(c);
      free(fss);
      return BadAlloc;
    }
    oc->fontname = (char *) xalloc(256);/* I don't want to deal with future reallocs errors */
    oc->origFontName = pfontname;
    oc->origFontNameLen = lenfname;
    if (!oc->fontname) {
      for (i = 0; i < c->num_fpes; i++)
        FreeFPE(c->fpe_list[i]);
      free(c->fpe_list);
      free(c);
      free(oc);
      free(fss);
      return BadAlloc;
    }
    /*
     * copy the current FPE list, so that if it gets changed by another client
     * while we're blocking, the request still appears atomic
     */
    oc->fpe_list = (FontPathElementPtr *)
	xalloc(sizeof(FontPathElementPtr) * num_fpes);
    if (!oc->fpe_list) {
	free(oc->fontname);
	free(oc);
      for (i = 0; i < c->num_fpes; i++)
         FreeFPE(c->fpe_list[i]);
       free(c->fpe_list);
       free(c);
       free(fss);
       return BadAlloc;
    }
    memmove(oc->fontname, pfontname, lenfname);
    for (i = 0; i < num_fpes; i++) {
	oc->fpe_list[i] = font_path_elements[i];
	UseFPE(oc->fpe_list[i]);
    }
    oc->client = client;
    oc->fontid = fid;
    oc->current_fpe = 0;
    oc->num_fpes = num_fpes;
    oc->fnamelen = lenfname;
    oc->slept = FALSE;
    oc->flags = flags;
    oc->non_cachable_font = cached;
    fss->c=c;
    fss->oc=oc;
    nxdoListFontsAndAliases(client, fss);
    return Success;
}
