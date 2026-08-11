/**
 * Hello Wii - A basic Wii Homebrew application
 * Displays "Hello from my Wii!" on screen using libogc console.
 *
 * Build requires: devkitPPC + libogc (from devkitPro)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

// Framebuffer and render mode
static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

int main(int argc, char **argv) {
    // Initialize the video subsystem
    VIDEO_Init();

    // Initialize Wii remotes
    WPAD_Init();

    // Obtain the preferred video mode for the console
    rmode = VIDEO_GetPreferredMode(NULL);

    // Allocate the framebuffer in uncached memory (MEM_K0_TO_K1 converts
    // cached to uncached KSEG address)
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    // Initialize the console for printf output
    console_init(xfb,
                 20,                         // x offset (pixels)
                 20,                         // y offset (pixels)
                 rmode->fbWidth,             // width
                 rmode->xfbHeight,           // height
                 rmode->fbWidth * VI_DISPLAY_PIX_SZ);  // stride

    // Configure and start the video output
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();

    // Extra sync needed for non-interlaced (progressive scan) modes
    if (rmode->viTVMode & VI_NON_INTERLACE) {
        VIDEO_WaitVSync();
    }

    // Move cursor to top-left of the console area and print the greeting
    printf("\x1b[2;0H");  // ANSI escape: row 2, column 0
    printf("***************************************\n");
    printf("*                                     *\n");
    printf("*      Hello from my Wii!             *\n");
    printf("*                                     *\n");
    printf("***************************************\n");
    printf("\n");
    printf("  devkitPPC + libogc homebrew running.\n");
    printf("\n");
    printf("  Press HOME button to exit.\n");

    // Main loop: scan pads each frame, exit on HOME button
    while (1) {
        WPAD_ScanPads();

        u32 pressed = WPAD_ButtonsDown(0);  // Wiimote 0 (first controller)
        if (pressed & WPAD_BUTTON_HOME) {
            exit(0);
        }

        // Wait for vertical sync to prevent screen tearing
        VIDEO_WaitVSync();
    }

    return 0;
}
