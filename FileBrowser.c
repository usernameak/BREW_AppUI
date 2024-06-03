#include "FileBrowser.h"

#include "AppInst.h"
#include "AppUI.h"
#include <AEEStdLib.h>

struct FileEntryData {
    CAppUIFileBrowser *fileBrowser;
    FileInfo info;
};

static boolean HasFileExtension(const char *path, const char *extension) {
    path = STRRCHR(path, '.');
    if (path != NULL) {
        return STRCMP(path, extension) == 0;
    }
    return FALSE;
}

static void InstallAppEntryCallback(CMenuEntry *entry, void *data_) {
    struct FileEntryData *data = entry->parent->parent->userData;

    int status = AppInst_ExtractAppliFromFile(data->fileBrowser->app, data->info.szName);
    if (status == AEE_SUCCESS) {
        CAppUIApp_ShowMessage(data->fileBrowser->app, entry->parent->parent,
                              (const AECHAR *) u"Success. Please reboot.");
    } else {
        AECHAR msg[64];
        WSPRINTF(msg, sizeof(msg), (const AECHAR *) u"INSTALL FAILED: %d\n", status);
        CAppUIApp_ShowMessage(data->fileBrowser->app, entry->parent->parent, msg);
    }
}

static void OpenAsDirEntryCallback(CMenuEntry *entry, void *data_) {
    struct FileEntryData *data = entry->parent->parent->userData;

    CAppUIFileBrowser_ShowDirectory(data->fileBrowser, data->info.szName);
}

static CMenu *ShowFileSubmenuListEntry(CMenuEntry *entry, void *data_) {
    struct FileEntryData *data = (struct FileEntryData *) data_;

    data->fileBrowser->fileActionMenu.userData = data;

    return &data->fileBrowser->fileActionMenu;
}

static void SelectFileBrowserListEntry(CMenuEntry *entry, void *data_) {
    struct FileEntryData *data = (struct FileEntryData *) data_;
    if (data->info.attrib & AEE_FA_DIR) {
        CAppUIFileBrowser_ShowDirectory(data->fileBrowser, data->info.szName);
    }
}

static void ReleaseFileBrowserListEntry(CMenuEntry *entry, void *data) {
    FREE(data);
}

static void AddFileBrowserListEntryEx(CAppUIFileBrowser *self, CMenuEntryGroup *group,
                                      const AECHAR *name, FileInfo *info) {
    CMenuEntry *fileEntry = NULL;
    if (SUCCESS == CMenuEntryGroup_AddNewEntry(group, name, &fileEntry)) {
        struct FileEntryData *data = MALLOC(sizeof(struct FileEntryData));
        data->fileBrowser = self;
        data->info = *info;
        fileEntry->userData = data;
        fileEntry->releaseCallback = ReleaseFileBrowserListEntry;
        fileEntry->callback = SelectFileBrowserListEntry;
        if (!(info->attrib & AEE_FA_DIR)) {
            fileEntry->submenuCallback = ShowFileSubmenuListEntry;
        }
    }
}

static void AddFileBrowserListEntry(CAppUIFileBrowser *self, CMenuEntryGroup *group,
                                    FileInfo *info) {
    const char *aName = STRRCHR(info->szName, '/');
    if (aName == NULL) {
        aName = info->szName;
    } else {
        aName++;
    }
    AECHAR name[sizeof(info->szName)];
    STREXPAND((const byte *) aName, STRLEN(aName) + 1, name, sizeof(name));
    AddFileBrowserListEntryEx(self, group, name, info);
}

static boolean GetParentDirectoryInfo(const char *directory, FileInfo *info) {
    info->attrib = AEE_FA_DIR;
    const char *pLastSlash = STRRCHR(directory, '/');
    if (pLastSlash == NULL || STRCMP(directory, "fs:/") == 0) {
        STRCPY(info->szName, "fs:/");
        return FALSE;
    } else {
        STRNCPY(info->szName, directory, pLastSlash - directory);
        info->szName[pLastSlash - directory] = 0;
        if (STRCMP(info->szName, "fs:") == 0) {
            STRCPY(info->szName, "fs:/");
            return TRUE;
        }
    }
    info->dwSize = 0;
    info->dwCreationDate = 0;
    return TRUE;
}

void CAppUIFileBrowser_ShowDirectory(CAppUIFileBrowser *self, const char *directory) {
    CMenuEntryGroup_Clear(self->fileBrowserListGroup);

    STRCPY(self->fileBrowserCurrentDirectory, directory);

    FileInfo parentInfo;
    if (GetParentDirectoryInfo(directory, &parentInfo)) {
        AddFileBrowserListEntryEx(self, self->fileBrowserListGroup,
                                  (const AECHAR *) u"..", &parentInfo);
    }

    if (SUCCESS == IFILEMGR_EnumInit(self->fileMgr, directory, TRUE)) {
        FileInfo info;
        while (IFILEMGR_EnumNext(self->fileMgr, &info)) {
            // Sony Ericsson W53S has broken firmware in here and does not set that flag :(
            info.attrib |= AEE_FA_DIR;

            AddFileBrowserListEntry(self, self->fileBrowserListGroup, &info);
        }
    }
    if (SUCCESS == IFILEMGR_EnumInit(self->fileMgr, directory, FALSE)) {
        FileInfo info;
        while (IFILEMGR_EnumNext(self->fileMgr, &info)) {
            AddFileBrowserListEntry(self, self->fileBrowserListGroup, &info);
        }
    }
    if (STRCMP(directory, "fs:/") == 0) {
        FileInfo info = {0};
        STRCPY(info.szName, "fs:/card0");
        info.attrib = AEE_FA_DIR;
        AddFileBrowserListEntryEx(self, self->fileBrowserListGroup, (const AECHAR *)u"fs:/card0", &info);

        STRCPY(info.szName, "fs:/card0/pc");
        info.attrib = AEE_FA_DIR;
        AddFileBrowserListEntryEx(self, self->fileBrowserListGroup, (const AECHAR *)u"fs:/card0/pc", &info);
    }

    CMenuManager_OpenMenu(&self->app->menuManager, &self->fileBrowserMenu);
}

static void FileBrowserBackHandler(CMenuManager *manager, CMenu *menu, void *userData) {
    CAppUIFileBrowser *self = (CAppUIFileBrowser *) userData;
    FileInfo info;
    if (GetParentDirectoryInfo(self->fileBrowserCurrentDirectory, &info)) {
        CAppUIFileBrowser_ShowDirectory(self, info.szName);
    } else {
        CMenuManager_OpenMenu(&self->app->menuManager, &self->app->mainMenu);
    }
}

int CAppUIFileBrowser_Init(CAppUIFileBrowser *self, CAppUIApp *app) {
    self->app = app;

    if (SUCCESS != ISHELL_CreateInstance(self->app->applet.m_pIShell, AEECLSID_FILEMGR,
                                         (void **) &self->fileMgr))
        goto onError;

    {
        CMenu_Init(&self->fileBrowserMenu);

        if (SUCCESS != CMenu_AddNewGroup(&self->fileBrowserMenu, &self->fileBrowserListGroup))
            goto onError;
        self->fileBrowserListGroup->groupLabel = (const AECHAR *) u"Files";

        self->fileBrowserMenu.backHandler = FileBrowserBackHandler;
        self->fileBrowserMenu.userData = self;
    }

    {
        CMenu_Init(&self->fileActionMenu);

        CMenuEntryGroup *actionsGroup;
        if (SUCCESS != CMenu_AddNewGroup(&self->fileActionMenu, &actionsGroup)) goto onError;
        actionsGroup->groupLabel = (const AECHAR *) u"File Actions";

        CMenuEntry *installFromZipEntry = NULL;
        if (SUCCESS != CMenuEntryGroup_AddNewEntry(actionsGroup,
                                                   (const AECHAR *) u"Install from ZIP",
                                                   &installFromZipEntry))
            goto onError;
        installFromZipEntry->callback = InstallAppEntryCallback;

        CMenuEntry *openAsDirEntry = NULL;
        if (SUCCESS != CMenuEntryGroup_AddNewEntry(actionsGroup,
                                                   (const AECHAR *) u"Force open as dir.",
                                                   &openAsDirEntry))
            goto onError;
        openAsDirEntry->callback = OpenAsDirEntryCallback;

        self->fileActionMenu.backMenu = &self->fileBrowserMenu;
    }

    return TRUE;

    onError:
    CAppUIFileBrowser_Free(self);
    return FALSE;
}

void CAppUIFileBrowser_Free(CAppUIFileBrowser *self) {
    CMenu_Free(&self->fileBrowserMenu);
    if (self->fileMgr != NULL) {
        IFILEMGR_Release(self->fileMgr);
        self->fileMgr = NULL;
    }
}