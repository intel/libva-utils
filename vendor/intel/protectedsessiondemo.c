/*
 * Copyright (c) 2020 Intel Corporation. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * it is a real program to show how VAAPI protected session work,
 * It does protected session create/destroy, and execute firmware version query command.
 *
 * ./protectedsessiondemo  : execute query and output result.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <va/va.h>
#include <va/va_prot.h>
#include "va_display.h"

#define CHECK_VASTATUS(va_status,func)                                  \
if (va_status != VA_STATUS_SUCCESS) {                                   \
    fprintf(stderr,"%s: failed with error 0x%x, exit\n", func, va_status); \
    exit(1);                                                            \
}

#define INPUT_SIZE             4  // size of input array for getting fw version
#define OUTPUT_SIZE            23 // size of output array for getting fw version
#define FW_VERSION_OFFSET      9  // offset for fw version
#define EXEC_FUNCID    0x40001000 // function id for query cmd execution

int main(int argc,char **argv)
{
    int num_entrypoint = 0;
    int entrypoint = 0;
    VAConfigAttrib attrib = {0};
    VAConfigID config_id = 0;
    int major_ver = 0;
    int minor_ver = 0;
    VADisplay va_dpy = NULL;
    VAStatus va_status = VA_STATUS_ERROR_OPERATION_FAILED;
    VAProtectedSessionID protected_session = 0xFF;
    VABufferID buffer = 0;
    VAProtectedSessionExecuteBuffer exec_buff = {0};
    uint32_t input_get_fw[INPUT_SIZE];
    memset(input_get_fw, 0, sizeof(input_get_fw));
    uint32_t output_get_fw[OUTPUT_SIZE];
    memset(output_get_fw, 0, sizeof(output_get_fw));
    VAEntrypoint entrypoints[6];
    memset(entrypoints, 0, sizeof(entrypoints));

    va_init_display_args(&argc, argv);
    va_dpy = va_open_display();
    va_status = vaInitialize(va_dpy, &major_ver, &minor_ver);
    CHECK_VASTATUS(va_status, "vaInitialize");

    va_status = vaQueryConfigEntrypoints(
        va_dpy,
        VAProfileProtected,
        entrypoints,
        &num_entrypoint);
    CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints");

    for(entrypoint = 0; entrypoint < num_entrypoint; entrypoint++)
    {
        if (entrypoints[entrypoint] == VAEntrypointProtectedTEEComm)
            break;
    }
    if (entrypoint == num_entrypoint)
    {
        /* not find VAEntrypointProtectedTEEComm entry point */
        assert(0);
    }

    /* entrypoint found, find out VAConfigAttribProtectedTeeFwVersion is support */
    attrib.type = VAConfigAttribProtectedTeeFwVersion;
    va_status = vaGetConfigAttributes(
        va_dpy,
        VAProfileProtected,
        VAEntrypointProtectedTEEComm,
        &attrib,
        1);
    CHECK_VASTATUS(va_status, "vaGetConfigAttributes");
    if (!attrib.value)
    {
        printf("\nAttribute type VAConfigAttribProtectedTeeFwVersion is not supported\n");
    }

    /* create config*/
    va_status = vaCreateConfig(
        va_dpy,
        VAProfileProtected,
        VAEntrypointProtectedTEEComm,
        &attrib,
        1,
        &config_id);
    CHECK_VASTATUS(va_status, "vaCreateConfig");

    /* create protected session */
    va_status = vaCreateProtectedSession(va_dpy, config_id, &protected_session);
    CHECK_VASTATUS(va_status, "vaCreateProtectedSession");

    /* get fw version */
    /* first create a VA buffer with type VAProtectedSessionExecuteBufferType */
    /* then getting FW version via vaProtectedSessionExecute()*/
    input_get_fw[0] = 0x40002;
    input_get_fw[1] = 0x28;
    exec_buff.function_id = EXEC_FUNCID;
    exec_buff.input.data = input_get_fw;
    exec_buff.input.data_size = sizeof(input_get_fw);
    exec_buff.output.data = output_get_fw;
    exec_buff.output.data_size = sizeof(output_get_fw);
    exec_buff.output.max_data_size = sizeof(output_get_fw);
    va_status = vaCreateBuffer(
        va_dpy,
        protected_session,
        VAProtectedSessionExecuteBufferType,
        sizeof(exec_buff),
        1,
        &exec_buff,
        &buffer);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaProtectedSessionExecute(va_dpy, protected_session, buffer);
    CHECK_VASTATUS(va_status, "vaProtectedSessionExecute");
    vaDestroyBuffer(va_dpy, buffer);

    printf("\nFW version 0x%x\n", *(output_get_fw + FW_VERSION_OFFSET));

    /* destroy protected session */
	va_status = vaDestroyProtectedSession(va_dpy, protected_session);
    CHECK_VASTATUS(va_status, "vaDestroyProtectedSession");

    vaDestroyConfig(va_dpy,config_id);
    vaTerminate(va_dpy);
    va_close_display(va_dpy);
    return 0;
}
