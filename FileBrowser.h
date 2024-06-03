#pragma once

#include "Menu.h"
#include <AEEFile.h>

typedef struct CAppUIApp CAppUIApp;

typedef struct {
    CAppUIApp *app;
    CMenu fileBrowserMenu;
    CMenu fileActionMenu;
    CMenuEntryGroup *fileBrowserListGroup;
    IFileMgr *fileMgr;
    char fileBrowserCurrentDirectory[AEE_MAX_FILE_NAME];
} CAppUIFileBrowser;


void CAppUIFileBrowser_ShowDirectory(CAppUIFileBrowser *self, const char *directory);
int CAppUIFileBrowser_Init(CAppUIFileBrowser *self, CAppUIApp *app);
void CAppUIFileBrowser_Free(CAppUIFileBrowser *self);
