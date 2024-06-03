#pragma once

typedef struct CAppUIApp CAppUIApp;

void AppInst_ReloadModules(CAppUIApp *app);
int AppInst_ExtractAppliFromFile(CAppUIApp *app, const char *path);
