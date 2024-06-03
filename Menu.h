#pragma once

#include <AEEDisp.h>
#include <AEEShell.h>
#include <AEEText.h>
#include <AEE.h>

typedef struct CMenu CMenu;
typedef struct CMenuEntryGroup CMenuEntryGroup;
typedef struct CMenuEntry CMenuEntry;
typedef struct CMenuManager CMenuManager;

struct CMenuEntry {
    const AECHAR *label;

    void (*callback)(CMenuEntry *entry, void *);

    void (*releaseCallback)(CMenuEntry *entry, void *);

    CMenu *(*submenuCallback)(CMenuEntry *entry, void *);

    CMenuEntryGroup *parent;

    IImage *icon;
    boolean iconLoaded;
    AEEImageInfo iconInfo;

    ITextCtl *textCtl;

    void *userData;
};

void CMenuEntry_Init(CMenuEntry *self, CMenuEntryGroup *parent);

void CMenuEntry_SetTextInput(CMenuEntry *self, IShell *shell, const AECHAR *initialText);

void CMenuEntry_UpdateLabel(CMenuEntry *self, const AECHAR *label);

void CMenuEntry_SetIcon(CMenuEntry *self, IImage *icon);

void CMenuEntry_Free(CMenuEntry *self);

struct CMenuEntryGroup {
    const AECHAR *groupLabel;
    uint32 numEntries;
    uint32 capacity;
    CMenuEntry **entries;
    CMenu *parent;
};

void CMenuEntryGroup_Init(CMenuEntryGroup *self, CMenu *parent);

void CMenuEntryGroup_Sort(CMenuEntryGroup *self);

int CMenuEntryGroup_AddNewEntry(CMenuEntryGroup *self, const AECHAR *label, CMenuEntry **ppEntry);

void CMenuEntryGroup_Clear(CMenuEntryGroup *self);

void CMenuEntryGroup_Free(CMenuEntryGroup *self);

struct CMenu {
    uint32 numGroups;
    CMenuEntryGroup **groups;
    void *userData;
    CMenu *backMenu;

    void (*backHandler)(CMenuManager *manager, CMenu *menu, void *userData);
};

void CMenu_DefaultBackHandler(CMenuManager *manager, CMenu *menu, void *userData);

void CMenu_Init(CMenu *self);

int CMenu_AddNewGroup(CMenu *self, CMenuEntryGroup **ppGroup);

void CMenu_Free(CMenu *self);

struct CMenuManager {
    IShell *shell;
    IDisplay *display;
    int displayWidth, displayHeight;
    CMenu *currentMenu;
    CMenuEntry *currentMenuEntry;
    int scrollOffset;
};

int CMenuManager_Init(CMenuManager *self, IShell *shell);

void CMenuManager_Free(CMenuManager *self);

void CMenuManager_Render(CMenuManager *self);

void CMenuManager_OpenMenu(CMenuManager *self, CMenu *menu);

void CMenuManager_FixScroll(CMenuManager *self);

void CMenuManager_SelectEntry(CMenuManager *self, CMenuEntry *entry);

boolean CMenuManager_HandleEvent(CMenuManager *self, AEEEvent eCode, uint16 wParam, uint32 dwParam);
