/* $Id: TabsP.h,v 1.9 2000/01/27 19:04:29 falk Exp $
 *
 * TabsP.h - Private definitions for Index Tabs widget
 *
 */

#ifndef _MwTabsP_h
#define _MwTabsP_h

/***********************************************************************
 *
 * Tabs Widget Private Data
 *
 ***********************************************************************/

#include <X11/IntrinsicP.h>

#ifdef	USE_MOTIF
#include <Xm/XmP.h>
#include <Xm/PrimitiveP.h>
#include <Xm/ManagerP.h>
#endif

#include <Mowitz/MwTabs.h>

/****************************************************************
 *
 * class record declaration
 *
 ****************************************************************/

/* New fields for the Tabs widget class record */
typedef struct {XtPointer extension;} MwTabsClassPart;

/* Full class record declaration */
typedef struct _MwTabsClassRec {
    CoreClassPart	core_class;
    CompositeClassPart  composite_class;
    ConstraintClassPart	constraint_class;
#ifdef	USE_MOTIF
    XmManagerClassPart	manager_class;
#endif
    MwTabsClassPart	tabs_class;
} MwTabsClassRec;

extern MwTabsClassRec mwTabsClassRec;



/****************************************************************
 *
 * instance record declaration
 *
 ****************************************************************/

/* New fields for the MwTabs widget record */
typedef struct {
    /* resources */
    XFontStruct	*font ;
    Dimension   internalHeight, internalWidth ;
    Widget	topWidget ;
    XtCallbackList callbacks ;
    XtCallbackList popdownCallbacks ;
    Boolean	selectInsensitive ;
    Boolean	be_nice_to_cmap ;
#ifndef	USE_MOTIF
    Boolean	traversalOn ;
#endif
    int		top_shadow_contrast ;
    int		bot_shadow_contrast ;
    int		insensitive_contrast ;

    /* private state */
    Widget	tabChild ;		/* special child for drawing tabs */
    Widget	hilight ;		/* currently highlit child */
    Dimension	tab_height ;		/* height of tabs (all the same) */
    					/* Note: includes top shadow only */
    Dimension	tab_total ;		/* total height of all tabs */
    Dimension	child_width, child_height; /* child size, including borders */
    Dimension	max_cw, max_ch ;	/* max child preferred size */
    Cardinal	numRows ;
    XtGeometryMask last_query_mode;
    Boolean	needs_layout ;
} MwTabsPart;


typedef struct _MwTabsRec {
    CorePart		core;
    CompositePart	composite;
    ConstraintPart	constraint;
#ifdef	USE_MOTIF
    XmManagerPart	manager;
#endif
    MwTabsPart		tabs;
} MwTabsRec;




/****************************************************************
 *
 * constraint record declaration
 *
 ****************************************************************/

typedef	struct _MwTabsConstraintsPart {
	/* resources */
	String	label ;
	Pixmap	left_bitmap ;
	Pixel	foreground ;
	Boolean	resizable ;

	/* private state */
	Pixel		grey ;
	Boolean		greyAlloc ;
	Dimension	width ;		/* tab width */
	Position	x,y ;		/* tab base position */
	short		row ;		/* tab row */
	Position	l_x, l_y ;	/* label position */
	Position	lbm_x, lbm_y ;	/* bitmap position */
	unsigned int	lbm_width, lbm_height, lbm_depth ;
} MwTabsConstraintsPart ;

typedef	struct _MwTabsConstraintsRec {
#ifdef	USE_MOTIF
	XmManagerConstraintPart	manager;
#endif
	MwTabsConstraintsPart	tabs ;
} MwTabsConstraintsRec, *MwTabsConstraints ;




/****************************************************************
 *
 * Class record declaration for tabschild.  Private.
 *
 ****************************************************************/

/* New fields for the Tabs widget class record */
typedef struct {XtPointer extension;} TabsChildClassPart;

/* Full class record declaration */
typedef struct {
    CoreClassPart	core_class;
#ifdef	USE_MOTIF
    XmPrimitiveClassPart primitive_class;
#endif
    TabsChildClassPart	tabschild_class;
} TabsChildClassRec;



/****************************************************************
 *
 * instance record declaration
 *
 ****************************************************************/

/* New fields for the Tabs widget record */
typedef struct {
    /* resources taken from Tabs parent */

    /* private state */
    GC		foregroundGC ;
    GC		backgroundGC ;
    GC		greyGC ;
    GC		topGC ;			/* not used in motif */
    GC		botGC ;			/* not used in motif */
    Pixmap	grey50 ;		/* TODO: cache this elsewhere */
    XtPointer	extension ;
    Boolean	has_focus ;
} TabsChildPart;


typedef struct TabsChildWidget {
    CorePart		core;
#ifdef	USE_MOTIF
    XmPrimitivePart	primitive;
#endif
    TabsChildPart	tabschild;
} TabsChildRec;


#endif /* _MwTabsP_h */
