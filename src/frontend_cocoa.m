#import <Cocoa/Cocoa.h>
#import <AudioToolbox/AudioToolbox.h>
#import <Carbon/Carbon.h>
#include "frontend.h"
#include "gba.h"
#include <mach/mach_time.h>
#include <stdio.h>

static GBA* g_gba = NULL;
static bool g_fast_forward = false;
static AudioQueueRef g_audio_queue = NULL;
#define AUDIO_BUFFERS 4
#define AUDIO_BUF_SIZE 2048

static void audio_callback(void* custom_data, AudioQueueRef queue, AudioQueueBufferRef buffer) {
    (void)custom_data;
    if (!g_gba) return;

    int16_t* dest = (int16_t*)buffer->mAudioData;
    int samples_needed = buffer->mAudioDataBytesCapacity / (2 * sizeof(int16_t));
    int read = apu_read_samples(&g_gba->apu, dest, samples_needed);

    if (read < samples_needed) {
        memset(dest + read * 2, 0, (samples_needed - read) * 2 * sizeof(int16_t));
    }
    buffer->mAudioDataByteSize = samples_needed * 2 * sizeof(int16_t);
    AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
}

static void init_audio(void) {
    AudioStreamBasicDescription format;
    memset(&format, 0, sizeof(format));
    format.mSampleRate = 44100;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    format.mBitsPerChannel = 16;
    format.mChannelsPerFrame = 2;
    format.mBytesPerFrame = 4;
    format.mFramesPerPacket = 1;
    format.mBytesPerPacket = 4;

    OSStatus status = AudioQueueNewOutput(&format, audio_callback, NULL, NULL, NULL, 0, &g_audio_queue);
    if (status == noErr) {
        for (int i = 0; i < AUDIO_BUFFERS; i++) {
            AudioQueueBufferRef buf;
            AudioQueueAllocateBuffer(g_audio_queue, AUDIO_BUF_SIZE * 4, &buf);
            buf->mAudioDataByteSize = AUDIO_BUF_SIZE * 4;
            memset(buf->mAudioData, 0, buf->mAudioDataByteSize);
            AudioQueueEnqueueBuffer(g_audio_queue, buf, 0, NULL);
        }
        AudioQueueStart(g_audio_queue, NULL);
    }
}

@interface ZeffCanvasView : NSView
@end

@implementation ZeffCanvasView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    if (!g_gba) return;

    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextSetInterpolationQuality(ctx, kCGInterpolationNone);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider = CGDataProviderCreateWithData(
        NULL,
        g_gba->framebuffer,
        GBA_SCREEN_WIDTH * GBA_SCREEN_HEIGHT * 4,
        NULL
    );

    CGImageRef image = CGImageCreate(
        GBA_SCREEN_WIDTH,
        GBA_SCREEN_HEIGHT,
        8,
        32,
        GBA_SCREEN_WIDTH * 4,
        colorSpace,
        kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big,
        provider,
        NULL,
        false,
        kCGRenderingIntentDefault
    );

    NSRect bounds = [self bounds];
    CGContextDrawImage(ctx, NSRectToCGRect(bounds), image);

    CGImageRelease(image);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(colorSpace);
}

- (void)handleKey:(NSEvent *)event down:(BOOL)pressed {
    unsigned short code = [event keyCode];

    switch (code) {
        case kVK_ANSI_Z: // A
        case kVK_ANSI_X:
            keypad_set_key(&g_gba->keypad, KEY_A, pressed);
            if (pressed && !g_gba->intro_completed) g_gba->intro_frame_counter = 90;
            break;
        case kVK_ANSI_A: // B
        case kVK_ANSI_S:
            keypad_set_key(&g_gba->keypad, KEY_B, pressed);
            break;
        case kVK_Return: // Start
            keypad_set_key(&g_gba->keypad, KEY_START, pressed);
            if (pressed && !g_gba->intro_completed) g_gba->intro_frame_counter = 90;
            break;
        case kVK_Space:  // Select
        case kVK_Delete:
        case kVK_Shift:
            keypad_set_key(&g_gba->keypad, KEY_SELECT, pressed);
            break;
        case kVK_ANSI_Q: // L
            keypad_set_key(&g_gba->keypad, KEY_L, pressed);
            break;
        case kVK_ANSI_W: // R
        case kVK_ANSI_E:
            keypad_set_key(&g_gba->keypad, KEY_R, pressed);
            break;
        case kVK_UpArrow:
            keypad_set_key(&g_gba->keypad, KEY_UP, pressed);
            break;
        case kVK_DownArrow:
            keypad_set_key(&g_gba->keypad, KEY_DOWN, pressed);
            break;
        case kVK_LeftArrow:
            keypad_set_key(&g_gba->keypad, KEY_LEFT, pressed);
            break;
        case kVK_RightArrow:
            keypad_set_key(&g_gba->keypad, KEY_RIGHT, pressed);
            break;
        case kVK_Tab:
            if (pressed) g_fast_forward = !g_fast_forward;
            break;
        default:
            break;
    }
}

- (void)keyDown:(NSEvent *)event {
    [self handleKey:event down:YES];
}

- (void)keyUp:(NSEvent *)event {
    [self handleKey:event down:NO];
}

@end

@interface ZeffAppDelegate : NSObject <NSApplicationDelegate> {
    NSWindow* window;
    ZeffCanvasView* canvasView;
    NSTimer* frameTimer;
    uint64_t last_time;
    int frame_count;
    int max_frames;
}
- (instancetype)initWithMaxFrames:(int)maxFrames scale:(int)scale;
@end

@implementation ZeffAppDelegate

- (instancetype)initWithMaxFrames:(int)maxFrames scale:(int)scale {
    self = [super init];
    if (self) {
        self->max_frames = maxFrames;
        self->frame_count = 0;
        self->last_time = mach_absolute_time();

        int win_w = GBA_SCREEN_WIDTH * (scale > 0 ? scale : 3);
        int win_h = GBA_SCREEN_HEIGHT * (scale > 0 ? scale : 3);

        NSRect frame = NSMakeRect(200, 200, win_w, win_h);
        window = [[NSWindow alloc] initWithContentRect:frame
                                             styleMask:(NSWindowStyleMaskTitled |
                                                        NSWindowStyleMaskClosable |
                                                        NSWindowStyleMaskMiniaturizable |
                                                        NSWindowStyleMaskResizable)
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];

        [window setTitle:@"Zeff Station GBA"];
        [window setAcceptsMouseMovedEvents:YES];
        [window center];

        canvasView = [[ZeffCanvasView alloc] initWithFrame:frame];
        [window setContentView:canvasView];
        [window makeKeyAndOrderFront:nil];
        [window makeFirstResponder:canvasView];
    }
    return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    (void)aNotification;
    [NSApp activateIgnoringOtherApps:YES];

    init_audio();

    // 60 Hz frame timer
    frameTimer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 60.0)
                                                  target:self
                                                selector:@selector(renderFrame)
                                                userInfo:nil
                                                 repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:frameTimer forMode:NSRunLoopCommonModes];
}

- (void)renderFrame {
    if (!g_gba) return;

    int steps = g_fast_forward ? 4 : 1;
    for (int s = 0; s < steps; s++) {
        gba_run_frame(g_gba);
    }

    [canvasView setNeedsDisplay:YES];
    frame_count++;

    // Calculate FPS every 60 frames
    if (frame_count % 60 == 0) {
        uint64_t now = mach_absolute_time();
        mach_timebase_info_data_t info;
        mach_timebase_info(&info);
        uint64_t elapsed_ns = (now - last_time) * info.numer / info.denom;
        double fps = 60.0 / ((double)elapsed_ns / 1e9);
        last_time = now;

        NSString* title = [NSString stringWithFormat:@"Zeff Station - %s [%.1f FPS]%s",
                           (g_gba->rom_title[0] ? g_gba->rom_title : "ADVANCE"),
                           fps,
                           g_fast_forward ? " (TURBO)" : ""];
        [window setTitle:title];
    }

    if (max_frames > 0 && frame_count >= max_frames) {
        [NSApp terminate:nil];
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    (void)sender;
    return YES;
}

@end

int frontend_run(FrontendConfig* config) {
    GBA* gba = gba_create();
    if (!gba) {
        fprintf(stderr, "[Zeff Station] Failed to create GBA instance\n");
        return 1;
    }
    g_gba = gba;

    if (config->bios_path) {
        gba_load_bios(gba, config->bios_path);
    }

    if (config->rom_path) {
        if (!gba_load_rom(gba, config->rom_path)) {
            gba_destroy(gba);
            return 1;
        }
    }

    gba_reset(gba, config->skip_intro);

    if (config->headless) {
        printf("[Zeff Station] Running in headless mode for %d frames...\n",
               config->max_frames > 0 ? config->max_frames : 600);
        int total = config->max_frames > 0 ? config->max_frames : 600;
        for (int i = 0; i < total; i++) {
            gba_run_frame(gba);
            if ((i + 1) % 100 == 0) {
                printf("[Zeff Station] Frame %d/%d completed (CPU PC: 0x%08X)\n",
                       i + 1, total, gba->cpu.r[15]);
            }
        }
        printf("[Zeff Station] Headless execution finished successfully!\n");
        gba_destroy(gba);
        return 0;
    }

    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        ZeffAppDelegate* delegate = [[ZeffAppDelegate alloc] initWithMaxFrames:config->max_frames
                                                                         scale:config->scale];
        [app setDelegate:delegate];
        [app run];
    }

    if (g_audio_queue) {
        AudioQueueStop(g_audio_queue, true);
        AudioQueueDispose(g_audio_queue, true);
    }

    gba_destroy(gba);
    return 0;
}
