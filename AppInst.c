#include "AppInst.h"

#include "AppUI.h"
#include "miniz/miniz.h"

#include <AEEFile.h>
#include <AEEAppletCtl.h>

#include "MZFileUtils.h"

#include <AEEStdLib.h>

void AppInst_ReloadModules(CAppUIApp *app) {
#ifdef AEE_STATIC
    extern int AEE_LoadModuleList();

    AEE_LoadModuleList();
    IAppletCtl *appletCtl = NULL;
    if (SUCCESS == ISHELL_CreateInstance(app->applet.m_pIShell, AEECLSID_APPLETCTL, (void **)&appletCtl)) {
        int appletListSize = 0;
        int status         = IAPPLETCTL_GetRunningList(appletCtl, NULL, &appletListSize);
        if (status == SUCCESS || status == EBUFFERTOOSMALL) {
            uint32 *appletIDs = (uint32 *)MALLOC(appletListSize);
            status            = IAPPLETCTL_GetRunningList(appletCtl, appletIDs, &appletListSize);
            if (status == SUCCESS) {
                for (uint32 i = 0; i < appletListSize / sizeof(uint32); i++) {
                    DBGPRINTF("sending reload event to applet %08x", appletIDs[i]);
                    ISHELL_SendEvent(app->applet.m_pIShell, appletIDs[i], EVT_MOD_LIST_CHANGED, 0, 0);
                }
            }
        }
        IAPPLETCTL_Release(appletCtl);
    }
    ISHELL_Notify(app->applet.m_pIShell, AEECLSID_SHELL, NMASK_SHELL_MOD_LIST_CHANGED, 0);
#endif
}

int AppInst_ExtractAppliFromFile(CAppUIApp *app, const char *path) {
    IFileMgr *fm                         = NULL;
    IFile *file                          = NULL;
    IFile *ofile                         = NULL;
    FileInfo fileInfo                    = { 0 };
    int status                           = AEE_SUCCESS;
    char prefixedPath[AEE_MAX_FILE_NAME] = "fs:/";

    mz_zip_archive_file_stat fileStat = { 0 };

    mz_zip_archive zip = {
        .m_pAlloc   = MZFileUtils_MZAlloc,
        .m_pFree    = MZFileUtils_MZFree,
        .m_pRealloc = MZFileUtils_MZRealloc,
        .m_pRead    = MZFileUtils_MZRead,
    };

    status = ISHELL_CreateInstance(app->applet.m_pIShell, AEECLSID_FILEMGR, (void **)&fm);
    if (AEE_SUCCESS != status) goto fail;

    status = IFILEMGR_GetInfo(fm, path, &fileInfo);
    if (AEE_SUCCESS != status) goto fail;

    file = IFILEMGR_OpenFile(fm, path, _OFM_READ);
    if (file == NULL) {
        status = IFILEMGR_GetLastError(fm);
        goto fail;
    }

    zip.m_pIO_opaque = file;

    if (!mz_zip_reader_init(&zip, fileInfo.dwSize, 0)) {
        DBGPRINTF("mz_zip_reader_init failed: %d\n", mz_zip_peek_last_error(&zip));
        status = EFAILED;
        goto fail;
    }

    mz_uint numFiles = mz_zip_reader_get_num_files(&zip);

    for (int i = 0; i < numFiles; i++) {
        if (!mz_zip_reader_file_stat(&zip, i, &fileStat)) continue;

        // unsafe but who cares
        STRCPY(&prefixedPath[4], fileStat.m_filename);

        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            status = IFILEMGR_MkDir(fm, prefixedPath);
            if (AEE_SUCCESS != status) goto zfail;
        } else {
            IFILEMGR_Remove(fm, prefixedPath);
            ofile = IFILEMGR_OpenFile(fm, prefixedPath, _OFM_CREATE);
            if (ofile == NULL) {
                status = IFILEMGR_GetLastError(fm);
                goto zfail;
            }

            if (!mz_zip_reader_extract_to_callback(&zip, i, MZFileUtils_MZWrite, ofile, 0)) {
                DBGPRINTF("mz_zip_reader_extract_to_callback failed: %d\n", mz_zip_peek_last_error(&zip));
                status = EFAILED;
                goto fail;
            }

            IFILE_Release(ofile);
            ofile = NULL;
        }
    }

    AppInst_ReloadModules(app);

zfail:
    mz_zip_end(&zip);

fail:
    if (ofile != NULL) IFILE_Release(ofile);
    if (file != NULL) IFILE_Release(file);
    if (fm != NULL) IFILEMGR_Release(fm);
    return status;
}
