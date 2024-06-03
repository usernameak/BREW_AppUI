#include "AppBkup.h"

#include "AppUI.h"

#include <AEEFile.h>
#include <AEEStdLib.h>

#include "MZFileUtils.h"

struct DirectoryRecord {
    char filename[AEE_MAX_FILE_NAME];
    struct DirectoryRecord *next;
};

static int StoreDirectoryInArchive(mz_zip_archive *zip, IFileMgr *fm, const char *dirname) {
    if (SUCCESS == IFILEMGR_EnumInit(fm, dirname, FALSE)) {
        FileInfo fileInfo;
        while (IFILEMGR_EnumNext(fm, &fileInfo)) {
            // TODO: probably error checking
            MZFileUtils_AddBrewFilePath(zip, fm, fileInfo.szName);
        }
    }

    struct DirectoryRecord *recordFirst = NULL;
    struct DirectoryRecord *recordLast  = NULL;

    if (SUCCESS == IFILEMGR_EnumInit(fm, dirname, TRUE)) {
        FileInfo fileInfo;
        while (IFILEMGR_EnumNext(fm, &fileInfo)) {
            struct DirectoryRecord *recordPrev = recordLast;

            recordLast = MALLOC(sizeof(struct DirectoryRecord));
            STRNCPY(recordLast->filename, fileInfo.szName, sizeof(recordLast->filename));
            recordLast->filename[sizeof(recordLast->filename) - 1] = 0;
            recordLast->next                                       = NULL;

            if (recordPrev) {
                recordPrev->next = recordLast;
            } else {
                recordFirst = recordLast;
            }
        }
    }

    struct DirectoryRecord *recordCur = recordFirst;
    while (recordCur) {
        StoreDirectoryInArchive(zip, fm, recordCur->filename);

        recordCur = recordCur->next;
    }

    recordCur = recordFirst;
    while (recordCur) {
        struct DirectoryRecord *recordNext = recordCur->next;
        FREE(recordCur);

        recordCur = recordNext;
    }

    return SUCCESS;
}


int AppBkup_Backup(CAppUIApp *app, AEEAppInfo *appInfo) {
    int status = AEE_SUCCESS;

    IFileMgr *fm = NULL;

    char moduleName[AEE_MAX_FILE_NAME];
    char backupFileName[AEE_MAX_FILE_NAME];
    char modDirName[AEE_MAX_FILE_NAME];

    mz_zip_archive zip = {
        .m_pAlloc   = MZFileUtils_MZAlloc,
        .m_pFree    = MZFileUtils_MZFree,
        .m_pRealloc = MZFileUtils_MZRealloc,
        .m_pWrite   = MZFileUtils_MZWrite
    };

    // acquire the file manager
    status = ISHELL_CreateInstance(app->applet.m_pIShell, AEECLSID_FILEMGR, (void **)&fm);
    if (AEE_SUCCESS != status) goto fail;

    // extract filename from .mif path
    {
        const char *tmp = STRRCHR(appInfo->pszMIF, '/');
        if (tmp == NULL) {
            tmp = appInfo->pszMIF;
        } else {
            tmp++;
        }
        STRNCPY(moduleName, tmp, AEE_MAX_FILE_NAME);
        moduleName[AEE_MAX_FILE_NAME - 1] = 0;
    }

    // strip .mif file extension from moduleName
    {
        char *tmp2 = STRRCHR(moduleName, '.');
        if (tmp2) *tmp2 = 0;
    }

    // build a path to the backup output
    // SNPRINTF(backupFileName, AEE_MAX_FILE_NAME, "fs:/shared/%s.zip", moduleName);
    SNPRINTF(backupFileName, AEE_MAX_FILE_NAME, "fs:/card0/pc/%s.zip", moduleName);

    // build a path to the module directory
    SNPRINTF(modDirName, AEE_MAX_FILE_NAME, "fs:/mod/%s/", moduleName);

    // create the backup archive with replacement
    IFILEMGR_Remove(fm, backupFileName);
    IFile *wfile = IFILEMGR_OpenFile(fm, backupFileName, _OFM_CREATE);
    if (wfile == NULL) {
        status = IFILEMGR_GetLastError(fm);
        goto fail;
    }

    zip.m_pIO_opaque = wfile;

    if (!mz_zip_writer_init(&zip, 0)) {
        status = EFAILED;
        goto fail;
    }

    // TODO: probably add some error checking
    MZFileUtils_AddBrewFilePath(&zip, fm, appInfo->pszMIF);
    StoreDirectoryInArchive(&zip, fm, modDirName);

    if (!mz_zip_writer_finalize_archive(&zip)) {
        status = EFAILED;
        goto fail;
    }

zfail:
    mz_zip_end(&zip);

fail:
    if (wfile) IFILE_Release(wfile);
    if (fm) IFILEMGR_Release(fm);

    return status;
}
