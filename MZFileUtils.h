#pragma once

#include <stddef.h>

#undef EALREADY
#include "miniz/miniz.h"

void *MZFileUtils_MZAlloc(void *opaque, size_t items, size_t size);
void MZFileUtils_MZFree(void *opaque, void *address);
void *MZFileUtils_MZRealloc(void *opaque, void *address, size_t items, size_t size);
size_t MZFileUtils_MZRead(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n);
size_t MZFileUtils_MZWrite(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n);
mz_bool MZFileUtils_AddBrewFile(mz_zip_archive *pZip, const char *pArchive_name, IFile *file, const AEEFileInfo *fileInfo);
int MZFileUtils_AddBrewFilePath(mz_zip_archive *pZip, IFileMgr *fm, const char *path);
