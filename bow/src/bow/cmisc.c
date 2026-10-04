/*
 * cmisc.c
 *
 * Copyright (c) 1995-1997, John Kilburg <john@cs.unlv.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>


#include "BowP.h"

/*
 * SourceAddHooks
 */
int
SourceAddHooks(cres, shooks)
BowResources cres;
BowSourceHooks *shooks;
{
  BowSourceHooks *nhooks;

  myassert(shooks->name != NULL, "Protocol not set in data hooks.");

  myassert(shooks->init != NULL, "Init function is NULL");
  myassert(shooks->stop != NULL, "Stop function is NULL");
  myassert(shooks->destroy != NULL, "Destroy function is NULL");
  myassert(shooks->getdata != NULL, "Getdata function is NULL");

  nhooks = (BowSourceHooks *)MPCGet(cres->mp, sizeof(BowSourceHooks));
  memcpy(nhooks, shooks, sizeof(BowSourceHooks));

  GListAddHead(cres->sourcehooks, nhooks);

  return(0);
}

/*
 * RenderAddHooks
 */
int
RenderAddHooks(cres, rhooks)
BowResources cres;
BowRenderHooks *rhooks;
{
  BowRenderHooks *nr;

  nr = (BowRenderHooks *)MPCGet(cres->mp, sizeof(BowRenderHooks));
  memcpy(nr, rhooks, sizeof(BowRenderHooks));

  GListAddHead(cres->renderhooks, nr);

  return(0);
}

/*
 * RenderGetHooks
 */
BowRenderHooks *
RenderGetHooks(cres, content)
BowResources cres;
char *content;
{
  BowRenderHooks *rh;
  size_t rlen;
  GList list;

  if (content == NULL) return(NULL);

  rlen = strlen(content);

  list = cres->renderhooks;
  for (rh = (BowRenderHooks *)GListGetHead(list); rh != NULL;
       rh = (BowRenderHooks *)GListGetNext(list))
  {
    if (strlen(rh->content) == rlen && strcasecmp(rh->content, content) == 0)
    {
      return(rh);
    }
  }

  return(NULL);
}

/*
 * SourceGetHooks
 */
BowSourceHooks *
SourceGetHooks(cres, proto)
BowResources cres;
char *proto;
{
  BowSourceHooks *shooks;
  size_t plen;
  GList list;

  plen = strlen(proto);

  list = cres->sourcehooks;
  for (shooks = (BowSourceHooks *)GListGetHead(list); shooks != NULL;
       shooks = (BowSourceHooks *)GListGetNext(list))
  {
    if (strlen(shooks->name) == plen &&
	strcasecmp(shooks->name, proto) == 0)
    {
      return(shooks);
    }
  }

  return(NULL);
}

struct BowType
{
  char *content;
  char *ext;
};

static struct BowType def_mtlist[] =
{
  { "text/html", "html" },
  { "text/html", "htm" },
  { "text/plain", "txt" },
  { "image/gif", "gif" },
  { "image/xbm", "xbm" },
  { "image/x-svg", "svg" },
  { "image/svg", "svg" },
  { "image/x-xpixmap", "xpm" },
  { "image/x-portable-anymap", "pnm" },
  { "image/x-portable-bitmap", "pbm" },
  { "image/x-portable-graymap", "pgm" },
  { "image/x-portable-pixmap", "ppm" },
  { "image/jpeg", "jpg" },
  { "image/jpeg", "jpeg" },
  { "image/tiff", "tiff" },
  { "image/tiff", "tif" },
  { "image/x-fits", "fit" },
  { "image/x-fits", "fits" },
  { "image/x-fits", "fts" },
  { "image/x-png", "png" },
  { "image/png", "png" },
  { "audio/basic", "au" },
  { "audio/basic", "snd" },
  { "text/x-compress-html", "html.Z" },
  { "text/x-gzip-html", "html.gz" },
  { "application/postscript", "ps" },
  { "application/x-dvi", "dvi" },
  { "application/x-gzip", "gz" },
  { "application/x-compress", "Z" },
  { "application/x-tar", "tar" },
  { "video/mpeg", "mpeg" },
  { "video/mpeg", "mpg" },
  { NULL, NULL },
};

/*
 * BowReadTypes
 */
void
BowReadTypeFiles(cres, filelist)
BowResources cres;
char *filelist;
{
  FILE *fp;
  char *f;
  char *cp;
  char buffer[256];
  char content[256];
  char exts[256];
  struct BowType *c;
  char *filename, *e;
  MemPool mp;

  mp = MPCreate();

  cres->mimes = GListCreate();

  f = filelist;
  while ((filename = mystrtok(f, ':', &f)) != NULL)
  {
    filename = FixPath(mp, filename);
    if (filename == NULL) continue;

    fp = fopen(filename, "r");
    if (fp == NULL) continue;

    while (fgets(buffer, sizeof(buffer), fp))
    {
      if (buffer[0] == '#' || buffer[0] == '\n') continue;

      if (sscanf(buffer, "%s %[^\n]", content, exts) == 2)
      {
	cp = exts;
	while ((e = mystrtok(cp, ' ', &cp)) != NULL)
	{
	  c = (struct BowType *)malloc(sizeof(struct BowType));
	  memset(c, 0, sizeof(struct BowType));
	  c->content = strdup(content);
	  c->ext = malloc(strlen(e) + 2);
	  strcpy(c->ext, ".");
	  strcat(c->ext, e);

	  GListAddTail(cres->mimes, c);
	}
      }
    }

    fclose(fp);
  }

  MPDestroy(mp);

  return;
}

/*
 * BowExt2Content
 */
char *
BowExt2Content(cres, ext)
BowResources cres;
char *ext;
{
  int elen, flen;
  struct BowType *c;
  int i;
  GList list;

  list = cres->mimes;
  flen = strlen(ext);
  for (c = (struct BowType *)GListGetHead(list); c != NULL;
       c = (struct BowType *)GListGetNext(list))
  {
    elen = strlen(c->ext);
    if (elen > 0)
    {
      if (elen <= flen &&
	  strncasecmp(ext + flen - elen, c->ext, elen) == 0)
      {
	return(c->content);
      }
    }
  }

  for (i = 0; def_mtlist[i].content != NULL; i++)
  {
    elen = strlen(def_mtlist[i].ext);
    if (elen > 0)
    {
      if (elen <= flen &&
	  strncasecmp(ext + flen - elen, def_mtlist[i].ext, elen) == 0)
      {
	return(def_mtlist[i].content);
      }
    }
  }

  return(NULL);
}

void
CMethodVoidDoom()
{
  abort();
  return;
}

void *
CMethodVoidPtrDoom()
{
  abort();
  return(NULL);
}

int
CMethodIntDoom()
{
  abort();
  return(0);
}

char
CMethodCharDoom()
{
  abort();
  return(0);
}

char *
CMethodCharPtrDoom()
{
  abort();
  return(0);
}

byte *
CMethodBytePtrDoom()
{
  abort();
  return(0);
}

bool
CMethodBoolDoom()
{
  abort();
  return(0);
}
