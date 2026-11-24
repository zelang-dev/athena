/*
 * WWW.h
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
#ifndef __WWW_H__
#define __WWW_H__ 1

/*
 * Xt widget type definitions
 */
typedef struct _WWWClassRec  *WWWWidgetClass;
typedef struct _WWWRec	     *WWWWidget;

extern WidgetClass           wwwWidgetClass;

/*
 * Prototypes
 */
Widget WWWGetDrawWidget(Widget);
int WWWSetDrawSize(Widget, unsigned int, unsigned int);
int WWWGetDrawSize(Widget, unsigned int *, unsigned int *);
void WWWMoveChild(Widget, int, int);

void WWWSetScrollBar(Widget, bool);

void WWWSetResizeCallback(Widget, void (*)(), void *);
void WWWSetExposeCallback(Widget, void (*)(), void *);
void WWWSetSelectCallback(Widget, void (*)(), void *);
void WWWSetMotionCallback(Widget, void (*)(), void *);

void WWWGetScrollPosition(Widget, int *, int *);

#endif
