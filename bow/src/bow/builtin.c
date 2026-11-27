/*
 * builtin.c
 */

#include <stdio.h>
#include <X11/Intrinsic.h>
#include "Bow.h"

extern void InitModule_HTML(BowResources);
extern void InitModule_Plain(BowResources);
extern void InitModule_HTTP(BowResources);
extern void InitModule_HTTPS(BowResources);
extern void InitModule_FTP(BowResources);
extern void InitModule_File(BowResources);
extern void InitModule_Image(BowResources);
extern void InitModule_Mailto(BowResources);
extern void InitModule_Data(BowResources);

void InitBowBuiltins(BowResources cres) {
  InitModule_Plain(cres);
  InitModule_Image(cres);
  InitModule_HTML(cres);
  InitModule_HTTP(cres);
  InitModule_HTTPS(cres);
  InitModule_FTP(cres);
  InitModule_File(cres);
  InitModule_Mailto(cres);
  InitModule_Data(cres);
}
