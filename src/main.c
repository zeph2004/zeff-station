#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frontend.h"

static void print_banner(void) {
    printf("\n");
    printf("\033[1;36m");
    printf(" ███████╗███████╗███████╗███████╗    ███████╗████████╗ █████╗ ████████╗██╗ ██████╗ ███╗   ██╗\n");
    printf(" ╚══███╔╝██╔════╝██╔════╝██╔════╝    ██╔════╝╚══██╔══╝██╔══██╗╚══██╔══╝██║██╔═══██╗████╗  ██║\n");
    printf("   ███╔╝ █████╗  █████╗  █████╗      ███████╗   ██║   ███████║   ██║   ██║██║   ██║██╔██╗ ██║\n");
    printf("  ███╔╝  ██╔══╝  ██╔══╝  ██╔══╝      ╚════██║   ██║   ██╔══██║   ██║   ██║██║   ██║██║╚██╗██║\n");
    printf(" ███████╗███████╗██║     ██║         ███████║   ██║   ██║  ██║   ██║   ██║╚██████╔╝██║ ╚████║\n");
    printf(" ╚══════╝╚══════╝╚═╝     ╚═╝         ╚══════╝   ╚═╝   ╚═╝  ╚═╝   ╚═╝   ╚═╝ ╚═════╝ ╚═╝  ╚═══╝\n");
    printf("\033[1;33m                      G B A   E M U L A T I O N   S Y S T E M\033[0m\n\n");
}

static void print_usage(const char* prog) {
    printf("Usage: %s [options] <rom.gba>\n\n", prog);
    printf("Options:\n");
    printf("  --bios <path>       Path to custom BIOS file (optional)\n");
    printf("  --scale <factor>    Window scale factor 1-4 (default: 3 -> 720x480)\n");
    printf("  --skip-intro        Skip Zeff Station boot intro sequence\n");
    printf("  --headless          Run without display window (for headless tests)\n");
    printf("  --frames <n>        Stop execution after N frames (default: unlimited)\n");
    printf("  -h, --help          Show this help message\n\n");
    printf("Keyboard Controls:\n");
    printf("  A Button:           Z / X\n");
    printf("  B Button:           A / S\n");
    printf("  L / R Triggers:     Q / W\n");
    printf("  Start:              Return / Enter\n");
    printf("  Select:             Space / Backspace / Shift\n");
    printf("  D-Pad:              Arrow Keys (Up, Down, Left, Right)\n");
    printf("  Fast Forward:       Tab (Toggle 4x Turbo)\n\n");
}

int main(int argc, char* argv[]) {
    print_banner();

    FrontendConfig config;
    memset(&config, 0, sizeof(config));
    config.scale = 3;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--bios") == 0 && i + 1 < argc) {
            config.bios_path = argv[++i];
        } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            config.scale = atoi(argv[++i]);
            if (config.scale < 1) config.scale = 1;
            if (config.scale > 6) config.scale = 6;
        } else if (strcmp(argv[i], "--skip-intro") == 0) {
            config.skip_intro = true;
        } else if (strcmp(argv[i], "--headless") == 0) {
            config.headless = true;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            config.max_frames = atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            config.rom_path = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!config.rom_path && !config.headless) {
        printf("Note: No ROM specified. Starting Zeff Station BIOS mode...\n");
        printf("To load a ROM, pass the path as an argument: %s <path_to_game.gba>\n\n", argv[0]);
    }

    return frontend_run(&config);
}
