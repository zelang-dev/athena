/*
 * BowStream.h
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
#ifndef __BOWSTREAM_H__
#define __BOWSTREAM_H__ 1

typedef struct BowStreamP    *BowStream;

/*
 * io.c
 */
typedef void (*BowStreamCallback) _ArgProto((BowStream,
						 ssize_t, void *));

BowStream StreamCreateINet _ArgProto((BowResources, char *, int));
BowStream StreamCreateINet2 _ArgProto((BowResources));
BowStream StreamCreateFD _ArgProto((BowResources, int));
void StreamDestroy _ArgProto((BowStream));
void StreamWrite _ArgProto((BowStream, byte *, size_t,
			    BowStreamCallback, void *));
void StreamRead _ArgProto((BowStream, byte *, size_t,
			   BowStreamCallback, void *));
int StreamGetINetPort _ArgProto((BowStream));
unsigned long StreamGetINetAddr _ArgProto((BowStream));

#endif
