#include "Menu.h"

#include <AEEVCodes.h>
#include <AEEStdLib.h>

void CMenuEntry_Init(CMenuEntry *self, CMenuEntryGroup *parent) {
    self->label = NULL;
    self->callback = NULL;
    self->submenuCallback = NULL;
    self->releaseCallback = NULL;
    self->userData = NULL;
    self->icon = NULL;
    self->iconLoaded = FALSE;
    self->parent = parent;
    self->textCtl = NULL;
}

void CMenuEntry_UpdateLabel(CMenuEntry *self, const AECHAR *label) {
    FREE((void *) self->label);
    self->label = label ? WSTRDUP(label) : NULL;
}

void CMenuEntry_SetTextInput(CMenuEntry *self, IShell *shell, const AECHAR *initialText) {
    self->textCtl = NULL;
    if (SUCCESS == ISHELL_CreateInstance(shell, AEECLSID_TEXTCTL, (void **) &self->textCtl)) {
        AEERect rc = {0, 0, 0, 0};
        ITEXTCTL_SetProperties(self->textCtl, TP_FRAME | TP_FIXSETRECT);
        ITEXTCTL_SetRect(self->textCtl, &rc);
        if (initialText) {
            ITEXTCTL_SetText(self->textCtl, initialText, 0);
        }
    }
}

#if 0 // TODO: async icon loading needs to be fixed
static void CMenuEntry_RetrieveIconInfo(void *pUser, IImage *icon, AEEImageInfo *info, int nErr) {
    CMenuEntry *self = (CMenuEntry *) pUser;
    if (nErr == SUCCESS) {
        if (icon == self->icon) {
            self->iconLoaded = TRUE;
            self->iconInfo = *info;
        }
    } else {
        if (icon == self->icon) {
            if (self->icon) {
                IImage_Release(icon);
            }
            self->icon = NULL;
            self->iconLoaded = FALSE;
        }
    }
}
#endif

void CMenuEntry_SetIcon(CMenuEntry *self, IImage *icon) {
    if (self->icon) {
        IImage_Release(self->icon);
    }
    self->icon = NULL;
    self->iconLoaded = FALSE;
    if (icon) {
        IImage_AddRef(icon);
        self->icon = icon;
        self->iconLoaded = TRUE;
        IImage_GetInfo(self->icon, &self->iconInfo);
        // IImage_Notify(icon, CMenuEntry_RetrieveIconInfo, self);
    }
}

void CMenuEntry_Free(CMenuEntry *self) {
    if (self->releaseCallback) {
        self->releaseCallback(self, self->userData);
        self->releaseCallback = NULL;
    }
    FREE((void *) self->label);
    self->submenuCallback = NULL;
    self->callback = NULL;
    self->userData = NULL;
    self->label = NULL;
    if (self->textCtl) {
        ITEXTCTL_Release(self->textCtl);
        self->textCtl = NULL;
    }
    if (self->icon) {
        IImage_Release(self->icon);
        self->icon = NULL;
    }
    self->iconLoaded = FALSE;
}

void CMenuEntryGroup_Init(CMenuEntryGroup *self, CMenu *parent) {
    self->groupLabel = NULL;
    self->entries = NULL;
    self->numEntries = 0;
    self->capacity = 0;
    self->parent = parent;
}

static int CMenuEntryGroup_Sort_Compare(const void *a, const void *b) {
    CMenuEntry *aa = *(CMenuEntry **) a;
    CMenuEntry *bb = *(CMenuEntry **) b;

    return WSTRICMP(aa->label, bb->label);
}

void CMenuEntryGroup_Sort(CMenuEntryGroup *self) {
    QSORT(self->entries, self->numEntries, sizeof(CMenuEntry *), CMenuEntryGroup_Sort_Compare);
}

int CMenuEntryGroup_AddNewEntry(CMenuEntryGroup *self, const AECHAR *label, CMenuEntry **ppEntry) {
    CMenuEntry *entry = MALLOC(sizeof(CMenuEntry));
    if (entry == NULL) {
        return ENOMEMORY;
    }
    CMenuEntry_Init(entry, self);
    CMenuEntry_UpdateLabel(entry, label);

    uint32 newNumEntries = self->numEntries + 1;
    if (newNumEntries > self->capacity) {
        uint32 newCapacity = self->capacity + (self->capacity >> 1u);
        if (newCapacity <= self->capacity) newCapacity = self->capacity + 1;
        CMenuEntry **newEntries = REALLOC(self->entries, newCapacity * sizeof(CMenuEntry *));
        if (newEntries == NULL) {
            CMenuEntry_Free(entry);
            FREE(entry);
            return ENOMEMORY;
        }
        self->capacity = newCapacity;
        self->entries = newEntries;
    }

    self->entries[newNumEntries - 1] = entry;
    self->numEntries = newNumEntries;

    *ppEntry = entry;

    return SUCCESS;
}

void CMenuEntryGroup_Clear(CMenuEntryGroup *self) {
    FREE(self->entries);
    self->entries = NULL;
    self->numEntries = 0;
    self->capacity = 0;
}

void CMenuEntryGroup_Free(CMenuEntryGroup *self) {
    self->groupLabel = NULL;
    CMenuEntryGroup_Clear(self);
}

void CMenu_DefaultBackHandler(CMenuManager *manager, CMenu *menu, void *userData) {
    if (menu->backMenu) CMenuManager_OpenMenu(manager, menu->backMenu);
}

void CMenu_Init(CMenu *self) {
    self->numGroups = 0;
    self->groups = NULL;
    self->userData = NULL;
    self->backMenu = NULL;
    self->backHandler = CMenu_DefaultBackHandler;
}

int CMenu_AddNewGroup(CMenu *self, CMenuEntryGroup **ppGroup) {
    CMenuEntryGroup *group = MALLOC(sizeof(CMenuEntryGroup));
    if (group == NULL) {
        return ENOMEMORY;
    }
    CMenuEntryGroup_Init(group, self);

    uint32 newNumGroups = self->numGroups + 1;
    CMenuEntryGroup **newGroups = REALLOC(self->groups, newNumGroups * sizeof(CMenuEntryGroup *));
    if (newGroups == NULL) {
        CMenuEntryGroup_Free(group);
        FREE(group);
        return ENOMEMORY;
    }
    self->groups = newGroups;
    self->groups[newNumGroups - 1] = group;
    self->numGroups = newNumGroups;

    *ppGroup = group;

    return SUCCESS;
}

void CMenu_Free(CMenu *self) {
    for (uint32 i = 0; i < self->numGroups; i++) {
        FREE(self->groups[i]);
    }
    FREE(self->groups);

    self->groups = NULL;
    self->numGroups = 0;
    self->userData = NULL;
}

int CMenuManager_Init(CMenuManager *self, IShell *shell) {
    self->currentMenu = NULL;
    self->currentMenuEntry = NULL;
    self->shell = shell;
    ISHELL_AddRef(self->shell);

    AEEDeviceInfo devInfo;
    devInfo.wStructSize = sizeof(devInfo);
    ISHELL_GetDeviceInfo(shell, &devInfo);

    self->displayWidth = devInfo.cxScreen;
    self->displayHeight = devInfo.cyScreen;
    self->scrollOffset = 0;
    self->display = NULL;

    return ISHELL_CreateInstance(self->shell, AEECLSID_DISPLAY, (void **) &self->display);
}

void CMenuManager_Free(CMenuManager *self) {
    self->currentMenuEntry = NULL;
    self->currentMenu = NULL;
    IDisplay_Release(self->display);
    self->display = NULL;
    ISHELL_Release(self->shell);
    self->shell = NULL;
}

#define MENU_MANAGER_PADDING_X 2
#define MENU_MANAGER_PADDING_Y 2
#define MENU_MANAGER_TITLE_GAP 2

void CMenuManager_Render(CMenuManager *self) {
    IDisplay *disp = self->display;

    IDisplay_SetColor(disp, CLR_USER_BACKGROUND, RGB_BLACK);
    IDisplay_SetColor(disp, CLR_USER_TEXT, RGB_WHITE);

    IDisplay_FillRect(disp, NULL, RGB_BLACK);

    if (!self->currentMenu) return;

    int boldFontHeight = IDisplay_GetFontMetrics(disp, AEE_FONT_BOLD, NULL, NULL);
    int fontHeight = IDisplay_GetFontMetrics(disp, AEE_FONT_NORMAL, NULL, NULL);
    int y = -self->scrollOffset;
    for (uint32 i = 0; i < self->currentMenu->numGroups; i++) {
        CMenuEntryGroup *group = self->currentMenu->groups[i];
        {
            if (i != 0) {
                y += MENU_MANAGER_TITLE_GAP;
            }
            int h = MENU_MANAGER_PADDING_Y * 2 + boldFontHeight;
            AEERect rcText = {0, y, self->displayWidth, h};
            if (y + h >= 0 && y < self->displayHeight) {
                IDisplay_SetColor(disp, CLR_USER_BACKGROUND, 0x202020FF);
                int w = IDisplay_MeasureText(disp, AEE_FONT_BOLD, group->groupLabel);
                IDisplay_DrawText(disp, AEE_FONT_BOLD, group->groupLabel,
                                  -1, (self->displayWidth - w) / 2, y + MENU_MANAGER_PADDING_Y,
                                  &rcText,
                                  IDF_RECT_FILL);
            }
            y += h + MENU_MANAGER_TITLE_GAP;
        }
        for (uint32 j = 0; j < group->numEntries; j++) {
            CMenuEntry *entry = group->entries[j];
            int h = MENU_MANAGER_PADDING_Y * 2 + fontHeight;
            if (entry->iconLoaded && entry->icon) {
                int hWithIcon = MENU_MANAGER_PADDING_Y * 2 + entry->iconInfo.cy;
                if (hWithIcon > h) h = hWithIcon;
            }
            AEERect rcText = {0, y, self->displayWidth, h};
            if (y + h >= 0 && y < self->displayHeight) {
                if (entry->label) {
                    int x = MENU_MANAGER_PADDING_X;
                    int iconX = x;
                    if (entry->iconLoaded && entry->icon) {
                        x += entry->iconInfo.cx;
                        x += MENU_MANAGER_PADDING_X;
                    }
                    if (entry == self->currentMenuEntry) {
                        IDisplay_SetColor(disp, CLR_USER_BACKGROUND, 0xB5513FFF);
                    } else {
                        IDisplay_SetColor(disp, CLR_USER_BACKGROUND, RGB_BLACK);
                    }
                    IDisplay_DrawText(disp, AEE_FONT_NORMAL, (const AECHAR *) entry->label,
                                      -1, x, y + MENU_MANAGER_PADDING_Y, &rcText,
                                      IDF_RECT_FILL);

                    if (entry->iconLoaded && entry->icon) {
                        IImage_SetParm(entry->icon, IPARM_ROP, AEE_RO_TRANSPARENT, 0);
                        IImage_Draw(entry->icon, iconX, y + MENU_MANAGER_PADDING_Y);
                    }
                } else if (entry->textCtl) {
                    IDisplay_SetColor(disp, CLR_USER_BACKGROUND, RGB_BLACK);
                    ITEXTCTL_SetRect(entry->textCtl, &rcText);
                    ITEXTCTL_Redraw(entry->textCtl);
                }
            }

            y += h;
        }
    }
    IDisplay_Update(disp);
}

void CMenuManager_OpenMenu(CMenuManager *self, CMenu *menu) {
    self->currentMenuEntry = NULL;
    self->currentMenu = menu;
    self->scrollOffset = 0;

    if (menu->numGroups > 0) {
        if (menu->groups[0]->numEntries > 0) {
            self->currentMenuEntry = menu->groups[0]->entries[0];
        }
    }

    CMenuManager_Render(self);
}

void CMenuManager_FixScroll(CMenuManager *self) {
    IDisplay *disp = self->display;

    if (!self->currentMenu) return;

    int boldFontHeight = IDisplay_GetFontMetrics(disp, AEE_FONT_BOLD, NULL, NULL);
    int fontHeight = IDisplay_GetFontMetrics(disp, AEE_FONT_NORMAL, NULL, NULL);
    int y = 0;
    for (uint32 i = 0; i < self->currentMenu->numGroups; i++) {
        CMenuEntryGroup *group = self->currentMenu->groups[i];
        {
            if (i != 0) {
                y += MENU_MANAGER_TITLE_GAP;
            }
            AEERect rcText = {0, y, self->displayWidth,
                              MENU_MANAGER_PADDING_Y * 2 + boldFontHeight};
            y += rcText.dy + MENU_MANAGER_TITLE_GAP;
        }
        for (uint32 j = 0; j < group->numEntries; j++) {
            CMenuEntry *entry = group->entries[j];
            int h = MENU_MANAGER_PADDING_Y * 2 + fontHeight;
            if (entry->iconLoaded && entry->icon) {
                int hWithIcon = MENU_MANAGER_PADDING_Y * 2 + entry->iconInfo.cy;
                if (hWithIcon > h) h = hWithIcon;
            }
            if (entry == self->currentMenuEntry) {
                if (y < self->scrollOffset) {
                    self->scrollOffset = y;
                } else if (y + h - self->displayHeight >= self->scrollOffset) {
                    self->scrollOffset = y + h - self->displayHeight;
                }
                return;
            }
            y += h;
        }
    }
}

void CMenuManager_SelectEntry(CMenuManager *self, CMenuEntry *entry) {
    if (self->currentMenuEntry->textCtl) {
        ITEXTCTL_SetActive(self->currentMenuEntry->textCtl, FALSE);
    }
    self->currentMenuEntry = entry;
    CMenuManager_FixScroll(self);
    CMenuManager_Render(self);
}

boolean
CMenuManager_HandleEvent(CMenuManager *self, AEEEvent eCode, uint16 wParam, uint32 dwParam) {
    for (uint32 i = 0; i < self->currentMenu->numGroups; i++) {
        CMenuEntryGroup *group = self->currentMenu->groups[i];
        for (uint32 j = 0; j < group->numEntries; j++) {
            CMenuEntry *entry = group->entries[j];
            if (entry->textCtl && ITEXTCTL_HandleEvent(entry->textCtl, eCode, wParam, dwParam)) {
                return TRUE;
            }
        }
    }
    if (eCode == EVT_KEY) {
        if (wParam == AVK_DOWN) {
            boolean nextEntryIsNeeded = FALSE;
            for (uint32 i = 0; i < self->currentMenu->numGroups; i++) {
                CMenuEntryGroup *group = self->currentMenu->groups[i];
                for (uint32 j = 0; j < group->numEntries; j++) {
                    CMenuEntry *entry = group->entries[j];
                    if (nextEntryIsNeeded) {
                        CMenuManager_SelectEntry(self, entry);
                        return TRUE;
                    }
                    if (entry == self->currentMenuEntry) nextEntryIsNeeded = TRUE;
                }
            }
            return TRUE;
        } else if (wParam == AVK_UP) {
            CMenuEntry *lastEntry = NULL;
            for (uint32 i = 0; i < self->currentMenu->numGroups; i++) {
                CMenuEntryGroup *group = self->currentMenu->groups[i];
                for (uint32 j = 0; j < group->numEntries; j++) {
                    CMenuEntry *entry = group->entries[j];
                    if (entry == self->currentMenuEntry) {
                        if (lastEntry) {
                            CMenuManager_SelectEntry(self, lastEntry);
                        } else {
                            self->scrollOffset = 0;
                            CMenuManager_Render(self);
                        }
                        return TRUE;
                    }
                    lastEntry = entry;
                }
            }
            return TRUE;
        } else if (wParam == AVK_RIGHT) {
            boolean nextGroupIsNeeded = FALSE;
            for (uint32 i = 0; i < self->currentMenu->numGroups; i++) {
                CMenuEntryGroup *group = self->currentMenu->groups[i];
                if (nextGroupIsNeeded && group->numEntries > 0) {
                    CMenuManager_SelectEntry(self, group->entries[0]);
                    return TRUE;
                }
                for (uint32 j = 0; j < group->numEntries; j++) {
                    CMenuEntry *entry = group->entries[j];
                    if (entry == self->currentMenuEntry) nextGroupIsNeeded = TRUE;
                }
            }
            if (self->currentMenu->numGroups > 0) {
                CMenuEntryGroup *group = self->currentMenu->groups
                [self->currentMenu->numGroups - 1];
                if (group->numEntries > 0) {
                    self->currentMenuEntry = group->entries[group->numEntries - 1];
                    CMenuManager_FixScroll(self);
                    CMenuManager_Render(self);
                }
            }
            return TRUE;
        } else if (wParam == AVK_LEFT) {
            CMenuEntry *lastGroupFirstEntry = NULL;
            for (uint32 i = 0; i < self->currentMenu->numGroups; i++) {
                CMenuEntryGroup *group = self->currentMenu->groups[i];
                for (uint32 j = 0; j < group->numEntries; j++) {
                    CMenuEntry *entry = group->entries[j];
                    if (entry == self->currentMenuEntry) {
                        if (lastGroupFirstEntry) {
                            CMenuManager_SelectEntry(self, lastGroupFirstEntry);
                        } else {
                            self->scrollOffset = 0;
                            CMenuManager_Render(self);
                        }
                        return TRUE;
                    }
                }
                if (group->numEntries > 0) {
                    lastGroupFirstEntry = group->entries[0];
                }
            }
            return TRUE;
        } else if (wParam == AVK_SELECT) {
            if (self->currentMenuEntry && self->currentMenuEntry->callback) {
                self->currentMenuEntry->callback(self->currentMenuEntry,
                                                 self->currentMenuEntry->userData);
            } else if (self->currentMenuEntry && self->currentMenuEntry->textCtl) {
                ITEXTCTL_SetActive(self->currentMenuEntry->textCtl, TRUE);
            }
            return TRUE;
        } else if (wParam == AVK_SOFT1) {
            if (self->currentMenuEntry) {
                if (self->currentMenuEntry->submenuCallback) {
                    CMenu *menu = self->currentMenuEntry->submenuCallback(self->currentMenuEntry,
                                                                          self->currentMenuEntry->userData);
                    if (menu) {
                        CMenuManager_OpenMenu(self, menu);
                    }
                } else if (self->currentMenuEntry->callback) {
                    self->currentMenuEntry->callback(self->currentMenuEntry,
                                                     self->currentMenuEntry->userData);
                }
            }
            return TRUE;
        } else if (wParam == AVK_SOFT2 || wParam == AVK_CLR) {
            if (self->currentMenuEntry &&
                self->currentMenuEntry->textCtl &&
                ITEXTCTL_IsActive(self->currentMenuEntry->textCtl)) {
                ITEXTCTL_SetActive(self->currentMenuEntry->textCtl, FALSE);
            } else if (self->currentMenu && self->currentMenu->backHandler) {
                self->currentMenu->backHandler(self, self->currentMenu,
                                               self->currentMenu->userData);
            }
            return TRUE;
        }
    }
    return FALSE;
}
