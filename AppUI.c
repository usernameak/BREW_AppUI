#include "AppUI.h"

#include "AppBkup.h"

#include <AEEModGen.h>
#include <AEEHeap.h>
#include <AEEFile.h>
#include <AEEStdLib.h>
#include "AppInst.h"

typedef struct {
    CAppUIApp *app;
    AEECLSID classID;
} AppStartupInfo;

static void StartupApp(CMenuEntry *entry, void *info_) {
    AppStartupInfo *info = (AppStartupInfo *)info_;
    int status           = ISHELL_StartApplet(info->app->applet.m_pIShell, info->classID);
    if (SUCCESS != status) {
        AECHAR errorMessage[64];
        WSPRINTF(errorMessage, 64, (const AECHAR *)u"Cannot start applet (%d)", status);
        CAppUIApp_ShowMessage(info->app, &info->app->mainMenu, errorMessage);
    }
}

static void AppUI_FreeAppData(CAppUIApp *app);

static void AppUI_ReloadAppList(CAppUIApp *app);

static const AECHAR *const SHOW_HIDDEN_APPS_OFF = (const AECHAR *)u"Show Hidden Apps: off";
static const AECHAR *const SHOW_HIDDEN_APPS_ON  = (const AECHAR *)u"Show Hidden Apps: on";

static void ShowHiddenAppsEntryCallback(CMenuEntry *entry, void *app_) {
    CAppUIApp *app      = (CAppUIApp *)app_;
    app->showHiddenApps = !app->showHiddenApps;
    CMenuEntry_UpdateLabel(entry, app->showHiddenApps ? SHOW_HIDDEN_APPS_ON : SHOW_HIDDEN_APPS_OFF);
    AppUI_ReloadAppList(app);
    CMenuManager_FixScroll(&app->menuManager);
    CMenuManager_Render(&app->menuManager);
}

static void ShowSystemInformationEntryCallback(CMenuEntry *entry, void *app_) {
    CAppUIApp *app = (CAppUIApp *)app_;

    CMenuEntryGroup_Clear(app->ramStatusGroup);

    AECHAR strRamUsed[32];
    AECHAR strRamTotal[32];

    IHeap *heap = NULL;
    if (SUCCESS == ISHELL_CreateInstance(app->applet.m_pIShell, AEECLSID_HEAP, (void **)&heap)) {
        WSPRINTF(strRamUsed, sizeof(strRamUsed), (const AECHAR *)u"RAM Used: %dk",
            IHEAP_GetMemStats(heap) / 1024);
        IHEAP_Release(heap);
    } else {
        WSTRCPY(strRamUsed, (const AECHAR *)u"RAM Used: unknown");
    }

    AEEDeviceInfo deviceInfo;
    deviceInfo.wStructSize = sizeof(deviceInfo);
    ISHELL_GetDeviceInfo(app->applet.m_pIShell, &deviceInfo);

    WSPRINTF(strRamTotal, sizeof(strRamTotal), (const AECHAR *)u"RAM Total: %dk",
        deviceInfo.dwRAM / 1024);

    CMenuEntry *newEntry = NULL;
    CMenuEntryGroup_AddNewEntry(app->ramStatusGroup, strRamUsed, &newEntry);
    CMenuEntryGroup_AddNewEntry(app->ramStatusGroup, strRamTotal, &newEntry);
    CMenuManager_OpenMenu(&app->menuManager, &app->systemInformationMenu);
}

static void MessageEntryCallback(CMenuEntry *entry, void *app_) {
    CAppUIApp *app = (CAppUIApp *)app_;

    CMenu *menu = entry->parent->parent;
    if (menu->backHandler) menu->backHandler(&app->menuManager, menu, menu->userData);
}

void CAppUIApp_ShowMessage(CAppUIApp *app, CMenu *backMenu, const AECHAR *message) {
    CMenuEntryGroup_Clear(app->messageGroup);

    app->messageMenu.backMenu = backMenu;

    CMenuEntry *errorEntry = NULL;
    if (SUCCESS == CMenuEntryGroup_AddNewEntry(app->messageGroup, message, &errorEntry)) {
        errorEntry->userData = app;
        errorEntry->callback = MessageEntryCallback;
    }

    CMenuManager_OpenMenu(&app->menuManager, &app->messageMenu);
}

static void ShowFileBrowserEntryCallback(CMenuEntry *entry, void *app_) {
    CAppUIApp *app = (CAppUIApp *)app_;
    CAppUIFileBrowser_ShowDirectory(&app->fileBrowser, AEEFS_ROOT_DIR);
}

static void LaunchAppEntryCallback(CMenuEntry *entry, void *app_) {
    CAppUIApp *app = (CAppUIApp *)app_;

    CMenuManager_OpenMenu(&app->menuManager, &app->mainMenu);

    StartupApp(entry, entry->parent->parent->userData);
}

static void BackupAppEntryCallback(CMenuEntry *entry, void *app_) {
    CAppUIApp *app = (CAppUIApp *)app_;

    AppStartupInfo *info = (AppStartupInfo *)entry->parent->parent->userData;

    AEEAppInfo appInfo;
    if (ISHELL_QueryClass(app->applet.m_pIShell, info->classID, &appInfo)) {
        AppBkup_Backup(app, &appInfo);
    }

    CMenuManager_OpenMenu(&app->menuManager, &app->mainMenu);
}

static void ReloadModulesEntryCallback(CMenuEntry *entry, void *app_) {
    CAppUIApp *app = (CAppUIApp *)app_;

    AppInst_ReloadModules(app);
}

static void AppUI_MainBackHandler(CMenuManager *manager, CMenu *menu, void *userData) {
    CAppUIApp *app = (CAppUIApp *)userData;
    ISHELL_CloseApplet(app->applet.m_pIShell, FALSE);
}

static boolean AppUI_InitAppData(CAppUIApp *app) {
    /* try */ {
        if (SUCCESS != CMenuManager_Init(&app->menuManager, app->applet.m_pIShell)) goto onError;

        {
            CMenu_Init(&app->mainMenu);
            app->mainMenu.backHandler = AppUI_MainBackHandler;
            app->mainMenu.userData    = app;

            if (SUCCESS != CMenu_AddNewGroup(&app->mainMenu, &app->applicationsGroup)) goto onError;
            app->applicationsGroup->groupLabel = (const AECHAR *)u"Applications";

            CMenuEntryGroup *optionsGroup;
            if (SUCCESS != CMenu_AddNewGroup(&app->mainMenu, &optionsGroup)) goto onError;
            optionsGroup->groupLabel = (const AECHAR *)u"Options";

#if 0
            CMenuEntry *testEntry = NULL;
            if (SUCCESS != CMenuEntryGroup_AddNewEntry(optionsGroup, NULL, &testEntry)) goto onError;
            CMenuEntry_SetTextInput(testEntry, app->applet.m_pIShell, NULL);
#endif

            CMenuEntry *showHiddenAppsEntry = NULL;
            if (SUCCESS != CMenuEntryGroup_AddNewEntry(optionsGroup,
                               (const AECHAR *)u"Show Hidden Apps: off",
                               &showHiddenAppsEntry))
                goto onError;

            showHiddenAppsEntry->callback = ShowHiddenAppsEntryCallback;
            showHiddenAppsEntry->userData = app;

#ifdef AEE_STATIC
            CMenuEntry *reloadModules = NULL;
            if (SUCCESS != CMenuEntryGroup_AddNewEntry(optionsGroup,
                               (const AECHAR *)u"Reload Module List",
                               &reloadModules))
                goto onError;

            reloadModules->callback = ReloadModulesEntryCallback;
            reloadModules->userData = app;
#endif

            CMenuEntry *fileBrowserEntry = NULL;
            if (SUCCESS != CMenuEntryGroup_AddNewEntry(optionsGroup,
                               (const AECHAR *)u"File Manager...",
                               &fileBrowserEntry))
                goto onError;

            fileBrowserEntry->callback = ShowFileBrowserEntryCallback;
            fileBrowserEntry->userData = app;

            CMenuEntry *systemInfoEntry = NULL;
            if (SUCCESS != CMenuEntryGroup_AddNewEntry(optionsGroup,
                               (const AECHAR *)u"System Information...",
                               &systemInfoEntry))
                goto onError;

            systemInfoEntry->callback = ShowSystemInformationEntryCallback;
            systemInfoEntry->userData = app;
        }

        {
            CMenu_Init(&app->appActionMenu);

            CMenuEntryGroup *actionsGroup;
            if (SUCCESS != CMenu_AddNewGroup(&app->appActionMenu, &actionsGroup)) goto onError;
            actionsGroup->groupLabel = (const AECHAR *)u"Actions";

            CMenuEntry *launchAppEntry = NULL;
            if (SUCCESS != CMenuEntryGroup_AddNewEntry(actionsGroup,
                               (const AECHAR *)u"Launch App",
                               &launchAppEntry))
                goto onError;

            launchAppEntry->callback = LaunchAppEntryCallback;
            launchAppEntry->userData = app;

            CMenuEntry *backupAppEntry = NULL;
            if (SUCCESS != CMenuEntryGroup_AddNewEntry(actionsGroup,
                               (const AECHAR *)u"Backup App",
                               &backupAppEntry))
                goto onError;

            backupAppEntry->callback = BackupAppEntryCallback;
            backupAppEntry->userData = app;

            app->appActionMenu.backMenu = &app->mainMenu;
        }

        {
            CMenu_Init(&app->systemInformationMenu);

            CMenuEntryGroup *brewVersionGroup;
            if (SUCCESS != CMenu_AddNewGroup(&app->systemInformationMenu, &brewVersionGroup))
                goto onError;
            brewVersionGroup->groupLabel = (const AECHAR *)u"BREW Version";

            AECHAR versionInfo[32];
            GETAEEVERSION((byte *)versionInfo, sizeof(versionInfo), GAV_MSM | GAV_UPDATE);

            CMenuEntry *brewVersionEntry = NULL;
            CMenuEntryGroup_AddNewEntry(brewVersionGroup, versionInfo, &brewVersionEntry);

            if (SUCCESS != CMenu_AddNewGroup(&app->systemInformationMenu, &app->ramStatusGroup))
                goto onError;
            app->ramStatusGroup->groupLabel = (const AECHAR *)u"RAM Usage";

            app->systemInformationMenu.backMenu = &app->mainMenu;
        }

        {
            CMenu_Init(&app->messageMenu);

            if (SUCCESS != CMenu_AddNewGroup(&app->messageMenu, &app->messageGroup))
                goto onError;
            app->messageGroup->groupLabel = (const AECHAR *)u"Notice";
        }

        if (!CAppUIFileBrowser_Init(&app->fileBrowser, app)) goto onError;

        return TRUE;
    }
onError:
    AppUI_FreeAppData(app);
    return FALSE;
}

static void AppUI_FreeAppData(CAppUIApp *app) {
    app->applicationsGroup = NULL;
    CMenuManager_Free(&app->menuManager);
    CAppUIFileBrowser_Free(&app->fileBrowser);
    CMenu_Free(&app->mainMenu);
    CMenu_Free(&app->appActionMenu);
    CMenu_Free(&app->systemInformationMenu);
    CMenu_Free(&app->messageMenu);
}

static CMenu *ShowAppActions(CMenuEntry *entry, void *info_) {
    AppStartupInfo *info = (AppStartupInfo *)info_;

    info->app->appActionMenu.userData = info;

    return &info->app->appActionMenu;
}

static void ReleaseStartupInfo(CMenuEntry *entry, void *info) {
    FREE(info);
}

static const AEECLSID g_alwaysShownApps[] = {
    0x01007029, // Kyocera KCP hidden menu
    0x01007313, // Sony Ericsson hidden menu
    0
};

static void AppUI_ReloadAppList(CAppUIApp *app) {
    CMenuEntryGroup_Clear(app->applicationsGroup);

    IShell *shell = app->applet.m_pIShell;

    ISHELL_EnumAppletInit(shell);
    AEEAppInfo appInfo;
    while (ISHELL_EnumNextApplet(shell, &appInfo)) {
        if (!app->showHiddenApps) {
            boolean isAlwaysShown      = FALSE;
            const AEECLSID *alwaysShownAppId = g_alwaysShownApps;
            while (*alwaysShownAppId) {
                if (appInfo.cls == *alwaysShownAppId) {
                    isAlwaysShown = TRUE;
                    break;
                }
                alwaysShownAppId++;
            }
            if (!isAlwaysShown && (appInfo.wFlags & AFLAG_HIDDEN || appInfo.wFlags & AFLAG_SCREENSAVER)) {
                continue;
            }
        }
        if (appInfo.cls == app->applet.clsID) continue;

        AECHAR appName[64];
        if (!ISHELL_LoadResString(shell, appInfo.pszMIF, APPR_NAME(appInfo), appName,
                sizeof(appName))) {
            DBGPRINTF("Could not load app name for applet CLSID:0x%08x", appInfo.cls);
            continue;
        }

        IImage *icon = ISHELL_LoadResImage(shell, appInfo.pszMIF, APPR_THUMB(appInfo));

        AppStartupInfo *startupInfo = MALLOC(sizeof(AppStartupInfo));
        startupInfo->app            = app;
        startupInfo->classID        = appInfo.cls;

        CMenuEntry *entry = NULL;
        if (SUCCESS == CMenuEntryGroup_AddNewEntry(app->applicationsGroup, appName, &entry)) {
            CMenuEntry_SetIcon(entry, icon);
            entry->userData        = startupInfo;
            entry->releaseCallback = ReleaseStartupInfo;
            entry->submenuCallback = ShowAppActions;
            entry->callback        = StartupApp;
        }

        if (icon) IImage_Release(icon);
    }
    CMenuEntryGroup_Sort(app->applicationsGroup);
}

static boolean AppUI_HandleEvent(CAppUIApp *app, AEEEvent eCode, uint16 wParam, uint32 dwParam) {
    if (eCode == EVT_APP_START) {
        app->showHiddenApps = FALSE;
        AppUI_ReloadAppList(app);
        CMenuManager_OpenMenu(&app->menuManager, &app->mainMenu);
        return TRUE;
    } else if (eCode == EVT_APP_RESUME) {
        CMenuManager_Render(&app->menuManager);
        return TRUE;
    } else if (eCode == EVT_APP_SUSPEND) {
        return TRUE;
    } else if (eCode == EVT_APP_STOP) {
        return TRUE;
    } else if (CMenuManager_HandleEvent(&app->menuManager, eCode, wParam, dwParam)) {
        return TRUE;
    } else if (eCode == EVT_MOD_LIST_CHANGED) {
        AppUI_ReloadAppList(app);
        CMenuManager_Render(&app->menuManager);
        return TRUE;
    } else if (eCode == EVT_NOTIFY) {
        AEENotify *notify = (AEENotify *)dwParam;
        if (notify->cls == AEECLSID_SHELL && (notify->dwMask & NMASK_SHELL_INIT)) {
            ISHELL_StartApplet(app->applet.m_pIShell, app->applet.clsID);
            notify->st = NSTAT_PROCESSED;
        }
    }

    return FALSE;
}

int AppUI_CreateInstance(AEECLSID ClsId, IShell *pIShell, IModule *pMod, void **ppobj) {
    if (!AEEApplet_New(sizeof(CAppUIApp), ClsId, pIShell, pMod, (IApplet **)ppobj,
            (AEEHANDLER)AppUI_HandleEvent,
            (PFNFREEAPPDATA)AppUI_FreeAppData)) {
        return ENOMEMORY;
    }

    CAppUIApp *app = (CAppUIApp *)*ppobj;

    if (!AppUI_InitAppData(app)) {
        *ppobj = NULL;
        return EFAILED;
    }
    return SUCCESS;
}

#ifdef AEE_STATIC

int AppUI_Load(IShell *shell, void *ph, IModule *ppMod) {
    return AEEStaticMod_New(sizeof(AEEMod), shell, ph, ppMod,
        (PFNMODCREATEINST)AppUI_CreateInstance,
        (PFNFREEMODDATA)NULL);
}

#else

int AEEClsCreateInstance(AEECLSID ClsId, IShell *pIShell, IModule *po, void **ppObj) {
    return AppUI_CreateInstance(ClsId, pIShell, po, ppObj);
}

#endif