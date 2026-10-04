/*
 * BowStack.h
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
#ifndef __BOWSTACK_H__
#define __BOWSTACK_H__

typedef struct BowStackP *BowStack;

BowStack StackCreate _ArgProto((BowStack, int, int,
				    unsigned int, unsigned int,
				    BowRenderActionProc, void *));
char *StackGetCurrentURL _ArgProto((BowStack));
void StackBack _ArgProto((BowStack));
void StackForward _ArgProto((BowStack));
void StackHome _ArgProto((BowStack));
void StackOpen _ArgProto((BowStack, BowRequest *));
void StackReload _ArgProto((BowStack));
void StackCancel _ArgProto((BowStack));
void StackRedraw _ArgProto((BowStack));
void StackDestroy _ArgProto((BowStack));
BowStack StackFromGUI _ArgProto((BowContext, BowGUI));
BowStack StackGetParent _ArgProto((BowStack));
void StackAction _ArgProto((BowStack, BowRequest *, char *));

#endif
