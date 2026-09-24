#ifndef ZEFF_FRONTEND_H
#define ZEFF_FRONTEND_H

#include <stdbool.h>
#include "gba.h"

typedef struct FrontendConfig {
    const char* rom_path;
    const char* bios_path;
    int scale;
    bool fullscreen;
    bool headless;
    int max_frames;
    bool skip_intro;
} FrontendConfig;

int frontend_run(FrontendConfig* config);

#endif // ZEFF_FRONTEND_H
