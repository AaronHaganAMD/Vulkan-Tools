//
// File: vk_sdk_platform.h
//

/*
** Copyright (c) 2015 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

#ifndef VK_SDK_PLATFORM_H
#define VK_SDK_PLATFORM_H

#if defined(_WIN32)
#  define NOMINMAX
#  ifndef __cplusplus
#    undef inline
#    define inline __inline
#  endif // __cplusplus

// C99:
// Microsoft didn't implement C99 in Visual Studio; but started adding it with
// VS2013.  However, VS2013 still didn't have snprintf().  The following is a
// work-around (Note: The _CRT_SECURE_NO_WARNINGS macro must be set in the
// "CMakeLists.txt" file).
#define snprintf _snprintf
#define strdup _strdup

#endif // _WIN32

#endif // VK_SDK_PLATFORM_H
