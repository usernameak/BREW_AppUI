#include <AEEStdLib.h>
#include <AEEFile.h>

#include "MZFileUtils.h"

void *MZFileUtils_MZAlloc(void *opaque, size_t items, size_t size) {
    return MALLOC((items * size) | ALLOC_NO_ZMEM);
}

void MZFileUtils_MZFree(void *opaque, void *address) {
    FREE(address);
}

void *MZFileUtils_MZRealloc(void *opaque, void *address, size_t items, size_t size) {
    return REALLOC(address, (items * size) | ALLOC_NO_ZMEM);
}

size_t MZFileUtils_MZRead(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n) {
    IFile *file = pOpaque;
    if ((mz_int64)file_ofs < 0 || IFILE_Seek(file, _SEEK_START, (mz_int64)file_ofs) != AEE_SUCCESS)
        return 0;

    return IFILE_Read(file, pBuf, n);
}

size_t MZFileUtils_MZWrite(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n) {
    IFile *file = pOpaque;
    if ((mz_int64)file_ofs < 0 || IFILE_Seek(file, _SEEK_START, (mz_int64)file_ofs) != AEE_SUCCESS)
        return 0;

    return IFILE_Write(file, pBuf, n);
}

mz_bool MZFileUtils_AddBrewFile(mz_zip_archive *pZip, const char *pArchive_name, IFile *file, const AEEFileInfo *fileInfo) {
    const mz_dummy_time_t dummyTime = { 0 };
    return mz_zip_writer_add_read_buf_callback(
        pZip,
        pArchive_name,
        MZFileUtils_MZRead,
        file,
        fileInfo->dwSize,
        &dummyTime,
        NULL, 0,
        MZ_DEFAULT_COMPRESSION,
        NULL, 0,
        NULL, 0);
}

int MZFileUtils_AddBrewFilePath(mz_zip_archive *pZip, IFileMgr *fm, const char *path) {
    AEEFileInfo fileInfo;

    int status = IFILEMGR_GetInfo(fm, path, &fileInfo);
    if (status != AEE_SUCCESS) return status;

    IFile *file = IFILEMGR_OpenFile(fm, path, _OFM_READ);
    if (file == NULL) {
        return IFILEMGR_GetLastError(fm);
    }

    const char *strippedPath = path;
    if (STRNCMP(path, "fs:/", 4) == 0) {
        strippedPath = &path[4];
    }

    mz_bool result = MZFileUtils_AddBrewFile(pZip, strippedPath, file, &fileInfo);

    IFILE_Release(file);

    return result ? AEE_SUCCESS : AEE_EFAILED;
}
