/*!
 * \brief
 * Demonstrates how a main thread can create a FBC context, then create
 * a worker thread that performs the capture.
 *
 * It is similar to the NvFBCMultiThread sample application, except that
 * here the FBC context is shared between threads instead of having one
 * FBC context per thread.
 *
 * \file
 * This sample demonstrates the following features:
 * - Capture to system memory;
 * - Multi-threaded capture;
 * - Shared FBC context;
 * - Synchronous (blocking) capture.
 *
 *
 * \copyright
 * Copyright (c) 2014-2017, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <pthread.h>

#include <X11/Xlib.h>

#include <NvFBC.h>

#include "NvFBCUtils.h"

#define APP_VERSION 2

#define LIB_NVFBC_NAME "libnvidia-fbc.so.1"

#define N_FRAMES  10
#define N_THREADS 1

typedef struct
{
    unsigned int nFrames;
    NVFBC_SIZE frameSize;
} NvFBCThreadParams;

static void *libNVFBC = NULL;
static PNVFBCCREATEINSTANCE NvFBCCreateInstance_ptr = NULL;
static NVFBC_API_FUNCTION_LIST pFn;
unsigned char *frame = NULL;
NVFBC_SESSION_HANDLE fbcHandle;

/**
 * Creates and sets up a capture session to system memory.
 *
 * Captures a bunch of frames, converts them to BMP and saves them to the disk.
 *
 * This function is called per thread.
 */
static void th_entry_point(NvFBCThreadParams *th_params)
{
    unsigned int i;

    NVFBCSTATUS fbcStatus;

    NVFBC_BIND_CONTEXT_PARAMS bindParams;
    NVFBC_RELEASE_CONTEXT_PARAMS releaseParams;

    /*
     * The worker thread is about to use the FBC context, bind it.
     */
    memset(&bindParams, 0, sizeof(bindParams));

    bindParams.dwVersion = NVFBC_BIND_CONTEXT_PARAMS_VER;

    fbcStatus = pFn.nvFBCBindContext(fbcHandle, &bindParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return;
    }

    /*
     * Create a capture session to system memory.
     *
     * Pass the thread specific capture box and frame size.
     */
    printf("Worker thread: Capturing %u RGB frames of size %dx%d.\n",
           th_params->nFrames,
           th_params->frameSize.w,
           th_params->frameSize.h);

    for (i = 0; i < th_params->nFrames; i++) {
        char filename[64];

        int res;
        uint64_t t1, t2, t_grabbed_ms;

        NVFBC_TOSYS_GRAB_FRAME_PARAMS grabParams;

        NVFBC_FRAME_GRAB_INFO frameInfo;

        t1 = NvFBCUtilsGetTimeInMillis();

        memset(&grabParams, 0, sizeof(grabParams));
        memset(&frameInfo, 0, sizeof(frameInfo));

        grabParams.dwVersion = NVFBC_TOSYS_GRAB_FRAME_PARAMS_VER;

        /*
         * Use blocking calls.
         *
         * The application will wait for new frames.  New frames are generated
         * when the mouse cursor moves or when the screen if refreshed.
         */
        grabParams.dwFlags = NVFBC_TOSYS_GRAB_FLAGS_NOFLAGS;

        /*
         * This structure will contain information about the captured frame.
         */
        grabParams.pFrameGrabInfo = &frameInfo;

        /*
         * Capture a new frame.
         */
        fbcStatus = pFn.nvFBCToSysGrabFrame(fbcHandle, &grabParams);
        if (fbcStatus != NVFBC_SUCCESS) {
            fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
            goto done;
        }

        t2 = NvFBCUtilsGetTimeInMillis();

        t_grabbed_ms = t2 - t1;

        t1 = NvFBCUtilsGetTimeInMillis();

        sprintf(filename, "frame%u.bmp", frameInfo.dwCurrentFrame);

        /*
         * Convert RGB frame to BMP and save it on the disk.
         *
         * This operation can be quite slow.
         */
        res = NvFBCUtilsSaveFrame(NVFBC_BUFFER_FORMAT_RGB, filename, frame,
                                  frameInfo.dwWidth, frameInfo.dwHeight);
        if (res > 0) {
            fprintf(stderr, "Worker thread: Unable to save frame\n");
            goto done;
        }

        t2 = NvFBCUtilsGetTimeInMillis();

        printf("Worker thread: New frame id %u grabbed in %llu ms, "
               "saved in %llu ms.\n",
               frameInfo.dwCurrentFrame,
               (unsigned long long) t_grabbed_ms,
               (unsigned long long) (t2 - t1));
    }

done:
    /*
     * The worker thread is done using the FBC context, release it.
     */
    memset(&releaseParams, 0, sizeof(releaseParams));

    releaseParams.dwVersion = NVFBC_RELEASE_CONTEXT_PARAMS_VER;

    fbcStatus = pFn.nvFBCReleaseContext(fbcHandle, &releaseParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return;
    }
}

/**
 * Prints usage information.
 */
static void usage(const char *pname)
{
    printf("Usage: %s [options]\n", pname);
    printf("\n");
    printf("Options:\n");
    printf("  --help|-h\t\tThis message\n");
    printf("  --frames|-f <n>\tNumber of frames to capture (default: %u)\n",
           N_FRAMES);
}

/**
 * Initializes the NvFBC library and creates an NvFBC instance.
 *
 * Creates an NvFBC instance, then creates a worker thread to capture frames.
 */
int main(int argc, char *argv[])
{
    static struct option longopts[] = {
        { "frames", required_argument, NULL, 'f' },
        { NULL, 0, NULL, 0 }
    };

    int opt, res;
    unsigned int nFrames = N_FRAMES;
    unsigned int framebufferWidth, framebufferHeight;

    pthread_t th_id;
    NvFBCThreadParams th_params;

    NVFBCSTATUS fbcStatus;

    NVFBC_CREATE_HANDLE_PARAMS createHandleParams;
    NVFBC_GET_STATUS_PARAMS statusParams;
    NVFBC_CREATE_CAPTURE_SESSION_PARAMS createCaptureParams;
    NVFBC_DESTROY_CAPTURE_SESSION_PARAMS destroyCaptureParams;
    NVFBC_DESTROY_HANDLE_PARAMS destroyHandleParams;
    NVFBC_TOSYS_SETUP_PARAMS setupParams;
    NVFBC_RELEASE_CONTEXT_PARAMS releaseParams;
    NVFBC_BIND_CONTEXT_PARAMS bindParams;

    Display *dpy = NULL;

    /*
     * Parse the command line.
     */
    while ((opt = getopt_long(argc, argv, "hf:", longopts, NULL)) != -1) {
        switch (opt) {
            case 'f':
                nFrames = (unsigned int) atoi(optarg);
                break;
            case 'h':
            default:
                usage(argv[0]);
                return EXIT_SUCCESS;
        }
    }

    NvFBCUtilsPrintVersions(APP_VERSION);

    /*
     * Dynamically load the NvFBC library.
     */
    libNVFBC = dlopen(LIB_NVFBC_NAME, RTLD_NOW);
    if (libNVFBC == NULL) {
        fprintf(stderr, "Unable to open '%s'\n", LIB_NVFBC_NAME);
        return EXIT_FAILURE;
    }

    /*
     * Resolve the 'NvFBCCreateInstance' symbol that will allow us to get
     * the API function pointers.
     */
    NvFBCCreateInstance_ptr =
    (PNVFBCCREATEINSTANCE) dlsym(libNVFBC, "NvFBCCreateInstance");
    if (NvFBCCreateInstance_ptr == NULL) {
        fprintf(stderr, "Unable to resolve symbol 'NvFBCCreateInstance'\n");
        return EXIT_FAILURE;
    }

    /*
     * Create an NvFBC instance.
     *
     * API function pointers are accessible through pFn.
     */
    memset(&pFn, 0, sizeof(pFn));

    pFn.dwVersion = NVFBC_VERSION;

    fbcStatus = NvFBCCreateInstance_ptr(&pFn);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "Unable to create NvFBC instance (status: %d)\n",
                fbcStatus);
        return EXIT_FAILURE;
    }

    /*
     * Open X connection to retrieve the size of the framebuffer.
     */
    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        fprintf(stderr, "Unable to open display\n");
        return EXIT_FAILURE;
    }

    framebufferWidth  = DisplayWidth(dpy, XDefaultScreen(dpy));
    framebufferHeight = DisplayHeight(dpy, XDefaultScreen(dpy));

    /*
     * Create a session handle that is used to identify the client.
     */
    memset(&createHandleParams, 0, sizeof(createHandleParams));

    createHandleParams.dwVersion = NVFBC_CREATE_HANDLE_PARAMS_VER;

    fbcStatus = pFn.nvFBCCreateHandle(&fbcHandle, &createHandleParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return 1;
    }

    /*
     * Get information about the state of the display driver.
     *
     * This call is optional but helps the application decide what it should
     * do.
     */
    memset(&statusParams, 0, sizeof(statusParams));

    statusParams.dwVersion = NVFBC_GET_STATUS_PARAMS_VER;

    fbcStatus = pFn.nvFBCGetStatus(fbcHandle, &statusParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return 1;
    }

    if (statusParams.bCanCreateNow == NVFBC_FALSE) {
        fprintf(stderr, "It is not possible to create a capture session "
        "on this system.\n");
        return 1;
    }

    memset(&createCaptureParams, 0, sizeof(createCaptureParams));

    createCaptureParams.dwVersion     = NVFBC_CREATE_CAPTURE_SESSION_PARAMS_VER;
    createCaptureParams.eCaptureType  = NVFBC_CAPTURE_TO_SYS;
    createCaptureParams.bWithCursor   = NVFBC_TRUE;
    createCaptureParams.frameSize.w   = framebufferWidth;
    createCaptureParams.frameSize.h   = framebufferHeight;
    createCaptureParams.eTrackingType = NVFBC_TRACKING_DEFAULT;

    fbcStatus = pFn.nvFBCCreateCaptureSession(fbcHandle, &createCaptureParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return 1;
    }

    /*
     * Set up the capture session.
     *
     * The ppBuffer structure member will be allocated of the proper size by
     * the NvFBC library.
     */
    memset(&setupParams, 0, sizeof(setupParams));

    setupParams.dwVersion     = NVFBC_TOSYS_SETUP_PARAMS_VER;
    setupParams.eBufferFormat = NVFBC_BUFFER_FORMAT_RGB;
    setupParams.ppBuffer      = (void **) &frame;
    setupParams.bWithDiffMap  = NVFBC_FALSE;

    fbcStatus = pFn.nvFBCToSysSetUp(fbcHandle, &setupParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return 1;
    }

    /*
     * The main thread is about to hand work over the worker thread,
     * release the FBC context.
     */
    memset(&releaseParams, 0, sizeof(releaseParams));

    releaseParams.dwVersion = NVFBC_RELEASE_CONTEXT_PARAMS_VER;

    fbcStatus = pFn.nvFBCReleaseContext(fbcHandle, &releaseParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return 1;
    }

    /*
     * Create thread.
     */
    th_params.nFrames = nFrames;

    th_params.frameSize.w = framebufferWidth;
    th_params.frameSize.h = framebufferHeight;

    res = pthread_create(&th_id, NULL,
                         (void *) th_entry_point,
                         (void *) &th_params);
    if (res) {
        fprintf(stderr, "Unable to create worker thread (res: %d)\n", res);
        return EXIT_FAILURE;
    }

    res = pthread_join(th_id, NULL);
    if (res) {
        fprintf(stderr, "Unable to join worker thread (res: %d)\n", res);
        return EXIT_FAILURE;
    }

    /*
     * The main thread takes back the FBC context.
     */
    memset(&bindParams, 0, sizeof(bindParams));

    bindParams.dwVersion = NVFBC_BIND_CONTEXT_PARAMS_VER;

    fbcStatus = pFn.nvFBCBindContext(fbcHandle, &bindParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return 1;
    }

    /*
     * Destroy capture session, tear down resources.
     */
    memset(&destroyCaptureParams, 0, sizeof(destroyCaptureParams));

    destroyCaptureParams.dwVersion = NVFBC_DESTROY_CAPTURE_SESSION_PARAMS_VER;

    fbcStatus = pFn.nvFBCDestroyCaptureSession(fbcHandle, &destroyCaptureParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return 1;
    }

    /*
     * Destroy session handle, tear down more resources.
     */
    memset(&destroyHandleParams, 0, sizeof(destroyHandleParams));

    destroyHandleParams.dwVersion = NVFBC_DESTROY_HANDLE_PARAMS_VER;

    fbcStatus = pFn.nvFBCDestroyHandle(fbcHandle, &destroyHandleParams);
    if (fbcStatus != NVFBC_SUCCESS) {
        fprintf(stderr, "%s\n", pFn.nvFBCGetLastErrorStr(fbcHandle));
        return 1;
    }

    return EXIT_SUCCESS;
}

