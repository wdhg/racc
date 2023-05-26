#ifndef RACC_FIXINT_H
#define RACC_FIXINT_H

#include <limits.h>

typedef signed char i8;
typedef unsigned char u8;

#if SHRT_MAX == 32767
typedef signed short int i16;
typedef unsigned short int u16;
#elif INT_MAX == 32767
typedef signed int i16;
typedef unsigned int u16;
#elif LONG_MAX == 32767
typedef signed long i16;
typedef unsigned long u16;
#else
#error "Cannot find 16 bit integer."
#endif

#if SHRT_MAX == 2147483647
typedef signed short int i32;
typedef unsigned short int u32;
#elif INT_MAX == 2147483647
typedef signed int i32;
typedef unsigned int u32;
#elif LONG_MAX == 2147483647
typedef signed long i32;
typedef unsigned long u32;
#else
#error "Cannot find 32 bit integer."
#endif

#if SHRT_MAX == 9223372036854775807
typedef signed short int i64;
typedef unsigned short int u64;
#elif INT_MAX == 9223372036854775807
typedef signed int i64;
typedef unsigned int u64;
#elif LONG_MAX == 9223372036854775807
typedef signed long i64;
typedef unsigned long u64;
#else
#error "Cannot find 64 bit integer."
#endif

#endif
