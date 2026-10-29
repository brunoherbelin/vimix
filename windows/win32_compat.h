/*
 * Compatibility definitions for building vimix on Windows (MSYS2 UCRT64).
 * Force-included in every compilation unit by CMake on WIN32 only, to
 * provide the POSIX/glibc types and functions that the code uses freely.
 */
#ifndef VIMIX_WIN32_COMPAT_H
#define VIMIX_WIN32_COMPAT_H

#ifdef _WIN32

#include <sys/types.h>
#include <stdint.h>
#include <float.h>

typedef unsigned int  uint;
typedef unsigned long ulong;
typedef unsigned short ushort;
typedef uint64_t u_int64_t;
typedef uint32_t u_int32_t;

#ifndef MAXFLOAT
#define MAXFLOAT FLT_MAX
#endif

#endif /* _WIN32 */

#endif /* VIMIX_WIN32_COMPAT_H */
