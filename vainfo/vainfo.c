/*
 * Copyright (c) 2007 Intel Corporation. All Rights Reserved.
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <va/va_str.h>

#include "va_display.h"

#ifdef ANDROID

/* Macros generated from configure */
#define LIBVA_VERSION_S "2.0.0"

#endif

#define CHECK_VASTATUS(va_status,func, ret)                             \
if (va_status != VA_STATUS_SUCCESS) {                                   \
    fprintf(stderr,"%s failed with error code %d (%s),exit\n",func, va_status, vaErrorStr(va_status)); \
    ret_val = ret;                                                      \
    goto error;                                                         \
}

static void
usage_exit(const char *program)
{
    fprintf(stdout, "Show information from VA-API driver\n");
    fprintf(stdout, "Usage: %s --help\n", program);
    fprintf(stdout, "\t--help print this message\n\n");
    fprintf(stdout, "Usage: %s [options]\n", program);
    va_print_display_options(stdout);

    exit(0);
}

static void
parse_args(const char *name, int argc, char **argv)
{
    int c;
    int option_index = 0;

    static struct option long_options[] = {
        {"help",        no_argument,            0,      'h'},
        { NULL,         0,                      NULL,   0 }
    };

    va_init_display_args(&argc, argv);

    while ((c = getopt_long(argc, argv,
                            "",
                            long_options,
                            &option_index)) != -1) {
        switch(c) {
        case 'h':
        default:
            usage_exit(name);
            break;
        }
    }
}

int main(int argc, const char* argv[])
{
  VADisplay va_dpy;
  VAStatus va_status;
  int major_version, minor_version;
  const char *driver;
  const char *name = strrchr(argv[0], '/'); 
  VAProfile profile, *profile_list = NULL;
  int num_profiles, max_num_profiles, i;
  VAEntrypoint entrypoint, *entrypoints = NULL;
  int num_entrypoint = 0;
  int ret_val = 0;
  
  if (name)
      name++;
  else
      name = argv[0];

  parse_args(name, argc, (char **)argv);

  va_dpy = va_open_display();
  if (NULL == va_dpy)
  {
      fprintf(stderr, "%s: vaGetDisplay() failed\n", name);
      return 2;
  }
  
  va_status = vaInitialize(va_dpy, &major_version, &minor_version);
  CHECK_VASTATUS(va_status, "vaInitialize", 3);
  
  printf("%s: VA-API version: %d.%d (libva %s)\n",
         name, major_version, minor_version, LIBVA_VERSION_S);

  driver = vaQueryVendorString(va_dpy);
  printf("%s: Driver version: %s\n", name, driver ? driver : "<unknown>");

  num_entrypoint = vaMaxNumEntrypoints (va_dpy);
  entrypoints = malloc (num_entrypoint * sizeof (VAEntrypoint));
  if (!entrypoints) {
      printf ("Failed to allocate memory for entrypoint list\n");
      ret_val = -1;
      goto error;
  }

  printf("%s: Supported profile and entrypoints\n", name);
  max_num_profiles = vaMaxNumProfiles(va_dpy);
  profile_list = malloc(max_num_profiles * sizeof(VAProfile));

  if (!profile_list) {
      printf("Failed to allocate memory for profile list\n");
      ret_val = 5;
      goto error;
  }

  va_status = vaQueryConfigProfiles(va_dpy, profile_list, &num_profiles);
  CHECK_VASTATUS(va_status, "vaQueryConfigProfiles", 6);

  for (i = 0; i < num_profiles; i++) {
      profile = profile_list[i];
      va_status = vaQueryConfigEntrypoints(va_dpy, profile, entrypoints, 
                                           &num_entrypoint);
      if (va_status == VA_STATUS_ERROR_UNSUPPORTED_PROFILE)
	continue;

      CHECK_VASTATUS(va_status, "vaQueryConfigEntrypoints", 4);

      for (entrypoint = 0; entrypoint < num_entrypoint; entrypoint++) {
          printf("      %-32s:	%s\n",
                 vaProfileStr(profile),
                 vaEntrypointStr(entrypoints[entrypoint]));
      }
  }
  
error:
  free(entrypoints);
  free(profile_list);
  vaTerminate(va_dpy);
  va_close_display(va_dpy);
  
  return ret_val;
}
