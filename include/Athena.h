#ifndef _ATHENA_H
#define _ATHENA_H

#ifdef __linux__
#define _DEFAULT_SOURCE 1
#define ARROW_SCROLLBAR 1
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/X.h>
#include <X11/Xresource.h>
#include <X11/Xos.h>
#include <X11/Intrinsic.h>
#include <X11/Core.h>
#include <X11/Object.h>
#include <X11/Shell.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Xmu/Xmu.h>
#include <X11/Xmu/Converters.h>
#include <Mowitz.h>

#ifdef NATIVE_XAW
#	include <X11/Xaw/Box.h>
#	include <X11/Xaw/Paned.h>
#	include <X11/Xaw/Dialog.h>
#	include <X11/Xaw/Command.h>
#	include <X11/Xaw/Form.h>
#	include <X11/Xaw/AsciiText.h>

#	include <X11/Xaw/MenuButton.h>
#	include <X11/Xaw/Label.h>
#	include <X11/Xaw/Viewport.h>
#	include <X11/Xaw/List.h>
#	include <X11/Xaw/Scrollbar.h>
#	include <X11/Xaw/SimpleMenu.h>
#	include <X11/Xaw/SmeBSB.h>
#	include <X11/Xaw/SmeLine.h>
#	include <X11/Xaw/Repeater.h>
#else
#	include <Xaw95/Box.h>
#	include <Xaw95/Paned.h>
#	include <Xaw95/Dialog.h>
#	include <Xaw95/Command.h>
#	include <Xaw95/Form.h>
#	include <Xaw95/AsciiText.h>

#	include <Xaw95/MenuButton.h>
#	include <Xaw95/Label.h>
#	include <Xaw95/Viewport.h>
#	include <Xaw95/List.h>
#	include <Xaw95/Scrollbar.h>
#	include <Xaw95/SimpleMenu.h>
#	include <Xaw95/SmeBSB.h>
#	include <Xaw95/SmeLine.h>
#	include <Xaw95/Repeater.h>
#endif

#include <TextField.h>
#include <FileSelect.h>
#include <Gridbox.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <tls.h>
#endif /* _ATHENA_H */