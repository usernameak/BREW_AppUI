#pragma once

#include "Menu.h"
#include "FileBrowser.h"
#include <AEEAppGen.h>

typedef struct CAppUIApp {
    AEEApplet applet;
    CMenuManager menuManager;
    CMenu mainMenu;
    CMenu appActionMenu;
    CMenu systemInformationMenu;
    CMenu messageMenu;
    CMenuEntryGroup *applicationsGroup;
    CMenuEntryGroup *ramStatusGroup;
    CMenuEntryGroup *messageGroup;
    boolean showHiddenApps;
    CAppUIFileBrowser fileBrowser;
} CAppUIApp;

void CAppUIApp_ShowMessage(CAppUIApp *app, CMenu *backMenu, const AECHAR *message);
