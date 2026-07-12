/* Minimal C type/macro stubs so pycparser can parse commands.c without
   system headers. Only structure matters, not real definitions. */
typedef unsigned long size_t;
typedef long ssize_t;
typedef long time_t;
typedef char* va_list;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned char bool;
#define true 1
#define false 0
typedef struct FILE FILE;
#define NULL ((void*)0)
#define UINT32_MAX 0xffffffffU
#define UINT64_MAX 0xffffffffffffffffULL
#define SIZE_MAX ((size_t)-1)
#define INT_MAX 2147483647
#define PATH_MAX 4096
#define _GNU_SOURCE
#define __attribute__(x)
#define __extension__
#define __restrict
#define __restrict__
#define __inline__ inline
#define __const const
#define __signed signed
#define __volatile volatile
#define asm(x)
#define typeof(x) int
#define __builtin_va_list void*
#define __thread
#define _Alignof(x) 1
#define alignof(x) 1
#define __alignof__(x) 1
