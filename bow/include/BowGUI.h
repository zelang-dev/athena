/*
 * BowGUI.h
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
#ifndef __BOWGUI_H__
#define __BOWGUI_H__ 1
#include <X11/Intrinsic.h>

typedef struct BowGUIP *BowGUI;
typedef struct BowGUIScrollPosP *BowGUIScrollPos;

typedef void (*GUISizeCallback) _ArgProto((BowGUI, void *,
					   unsigned int, unsigned int));

BowGUI GUICreate _ArgProto((BowContext, BowGUI,
				GUISizeCallback, void *));
void GUIDestroy _ArgProto((BowGUI));

void GUIMap _ArgProto((BowGUI, int, int));
void GUIUnmap _ArgProto((BowGUI));

void GUISetInitialDimensions _ArgProto((BowGUI,
					unsigned int, unsigned int));

void GUIGetOnScreenDimensions _ArgProto((BowGUI,
					 int *, int *,
					 unsigned int *, unsigned int *));

int GUIGetDimensions _ArgProto((BowGUI,
				unsigned int *, unsigned int *));

void GUISetDimensions _ArgProto((BowGUI,
				 unsigned int, unsigned int));

Display *GUIToDisplay _ArgProto((BowGUI));
Window GUIToWindow _ArgProto((BowGUI));
Pixel GUIBackgroundPixel _ArgProto((BowGUI));
Widget GUIToWidget _ArgProto((BowGUI));

void GUISetScrollBar _ArgProto((BowGUI, bool));
void GUIGetScrollPosition _ArgProto((BowGUI, int *, int *));
void GUISetScrollPosition _ArgProto((BowGUI, int, int));

int GUIGetNamedColor _ArgProto((BowGUI, char *, Pixel *));

#endif
