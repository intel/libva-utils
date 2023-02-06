/*
 * Copyright © Microsoft Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef _COMPAT_WIN32_H_
#define _COMPAT_WIN32_H_

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The setenv() function returns zero on success, or -1 on error, with
* errno set to indicate the cause of the error.
*
* SetEnvironmentVariableA Return value
* If the function succeeds, the return value is nonzero.
* If the function fails, the return value is zero.
*/
inline int setenv(const char *varname, const char *value_string, int overwrite)
{
    return SetEnvironmentVariableA(varname, value_string) ? 0 : -1;
}

/* The unsetenv() function returns zero on success, or -1 on error,
* with errno set to indicate the cause of the error.
* SetEnvironmentVariableA Return value
* If the function succeeds, the return value is nonzero.
* If the function fails, the return value is zero.
*/
inline int unsetenv(const char *varname)
{
    return SetEnvironmentVariableA(varname, NULL) ? 0 : -1;
}

#ifdef __cplusplus
}
#endif

#endif /* _COMPAT_WIN32_H_ */
