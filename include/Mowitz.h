#ifndef MW_MOWITZ_H
#define MW_MOWITZ_H

#ifdef USE_DEBUG
#	undef MOWITZ_DATA
#	undef DEFAULT_PIXPATH
#	undef DEFAULT_DATAPATH
#	define MOWITZ_DATA "./share"
#	define DEFAULT_PIXPATH "./share/pixmaps"
#	define DEFAULT_DATAPATH MOWITZ_DATA
#else
#	ifndef MOWITZ_DATA
#		define MOWITZ_DATA "/usr/local/share/athena"
#	endif
#	ifndef DEFAULT_PIXPATH
#		define DEFAULT_PIXPATH "/usr/local/share/athena/pixmaps"
#	endif
#	ifndef DEFAULT_DATAPATH
#		define DEFAULT_DATAPATH MOWITZ_DATA
#	endif
#endif

#include <Mowitz/MwUtils.h>
#include <Mowitz/MwFormat.h>
#include <Mowitz/MwXutils.h>
#include <Mowitz/MwXFormat.h>
#include <Mowitz/MwAnimator.h>
#include <Mowitz/MwApplicationShell.h>
#include <Mowitz/MwBase.h>
#include <Mowitz/MwBaseComp.h>
#include <Mowitz/MwBaseConst.h>
#include <Mowitz/MwBaseME.h>
#include <Mowitz/MwButton.h>
#include <Mowitz/MwCanvas.h>
#include <Mowitz/MwCheck.h>
#include <Mowitz/MwCheckME.h>
#include <Mowitz/MwCombo.h>
#include <Mowitz/MwDialog.h>
#include <Mowitz/MwDND.h>
#include <Mowitz/MwFilesel.h>
#include <Mowitz/MwFrame.h>
#include <Mowitz/MwHandle.h>
#include <Mowitz/MwImage.h>
#include <Mowitz/MwLabelME.h>
#include <Mowitz/MwLineME.h>
#include <Mowitz/MwListTree.h>
#include <Mowitz/MwMBButton.h>
#include <Mowitz/MwMenu.h>
#include <Mowitz/MwMenuBar.h>
#include <Mowitz/MwMenuButton.h>
#include <Mowitz/MwNotebook.h>
#include <Mowitz/MwPopText.h>
#include <Mowitz/MwRichtext.h>
#include <Mowitz/MwRow.h>
#include <Mowitz/MwRudegrid.h>
#include <Mowitz/MwRuler.h>
#include <Mowitz/MwSButton.h>
#include <Mowitz/MwSlider.h>
#include <Mowitz/MwSpinner.h>
#include <Mowitz/MwSubME.h>
#include <Mowitz/MwTabbing.h>
#include <Mowitz/MwTable.h>
#include <Mowitz/MwTabs.h>
#include <Mowitz/MwTabstop.h>
#include <Mowitz/MwTextField.h>
#include <Mowitz/MwTooltip.h>
#include <Mowitz/MwTraverse.h>
#include <Mowitz/MwVSlider.h>
#include <Mowitz/MwXCC.h>
#include <Mowitz/MwHtml.h>
#endif	/* MW_MOWITZ_H */
