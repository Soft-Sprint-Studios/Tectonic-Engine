/*
 * Copyright 2026 Soft Sprint Studios
 *
 * This software is released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * This software is provided "as is", without any warranty or guarantee
 * of any kind, either expressed or implied.
 *
 * For more information, see <https://unlicense.org/>.
 */
#ifndef MINISPEC_H
#define MINISPEC_H

/*
    minispec - header-only system & CPU feature detection

    Usage:
        #define MINISPEC_IMPLEMENTATION
        #include "minispec.h"
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ------------------------------------------------------------
   CPU Feature API
   ------------------------------------------------------------ */

int minispec_has_rdtsc(void);
int minispec_has_cmov(void);
int minispec_has_fcmov(void);
int minispec_has_mmx(void);
int minispec_has_sse(void);
int minispec_has_sse2(void);
int minispec_has_sse3(void);
int minispec_has_ssse3(void);
int minispec_has_sse4a(void);
int minispec_has_sse41(void);
int minispec_has_sse42(void);
int minispec_has_3dnow(void);
int minispec_has_aes(void);
int minispec_has_avx(void);
int minispec_has_avx2(void);
int minispec_has_cmpxchg16b(void);
int minispec_has_lahf_sahf(void);

/* ------------------------------------------------------------
   System Info
   ------------------------------------------------------------ */

uint64_t minispec_memory_bytes(void);
uint32_t minispec_cpu_cores(void);
const char* minispec_cpu_vendor(void);
const char* minispec_cpu_brand(void);

#ifdef __cplusplus
}
#endif

/* ============================================================
   IMPLEMENTATION
   ============================================================ */
#ifdef MINISPEC_IMPLEMENTATION

#include <string.h>

/* Platform */
#if defined(_WIN32)
    #define MINISPEC_WINDOWS
    #include <windows.h>
    #include <intrin.h>
#elif defined(__APPLE__)
    #define MINISPEC_APPLE
    #include <unistd.h>
    #include <sys/sysctl.h>
#elif defined(__linux__)
    #define MINISPEC_LINUX
    #include <unistd.h>
#endif

/* Architecture */
#if defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
    #define MINISPEC_X86 1
#else
    #define MINISPEC_X86 0
#endif

/* ------------------------------------------------------------
   CPUID helpers
   ------------------------------------------------------------ */

#if MINISPEC_X86

static void minispec_cpuid(int out[4], int leaf)
{
#if defined(_MSC_VER)
    __cpuid(out, leaf);
#else
    __asm__ __volatile__(
        "cpuid"
        : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3])
        : "a"(leaf)
    );
#endif
}

static void minispec_cpuid_ex(int out[4], int leaf, int subleaf)
{
#if defined(_MSC_VER)
    __cpuidex(out, leaf, subleaf);
#else
    __asm__ __volatile__(
        "cpuid"
        : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3])
        : "a"(leaf), "c"(subleaf)
    );
#endif
}

#endif /* MINISPEC_X86 */

/* Feature helpers */

static int minispec_leaf1_ecx(int bit)
{
#if MINISPEC_X86
    int r[4]; minispec_cpuid(r, 1);
    return (r[2] & (1 << bit)) != 0;
#else
    return 0;
#endif
}

static int minispec_leaf1_edx(int bit)
{
#if MINISPEC_X86
    int r[4]; minispec_cpuid(r, 1);
    return (r[3] & (1 << bit)) != 0;
#else
    return 0;
#endif
}

static int minispec_leaf7_ebx(int bit)
{
#if MINISPEC_X86
    int r[4]; minispec_cpuid_ex(r, 7, 0);
    return (r[1] & (1 << bit)) != 0;
#else
    return 0;
#endif
}

static int minispec_leaf_ext_ecx(int bit)
{
#if MINISPEC_X86
    int r[4]; minispec_cpuid(r, 0x80000001);
    return (r[2] & (1 << bit)) != 0;
#else
    return 0;
#endif
}

static int minispec_leaf_ext_edx(int bit)
{
#if MINISPEC_X86
    int r[4]; minispec_cpuid(r, 0x80000001);
    return (r[3] & (1 << bit)) != 0;
#else
    return 0;
#endif
}

/* ------------------------------------------------------------
   Feature implementations
   ------------------------------------------------------------ */

int minispec_has_rdtsc(void)      { return minispec_leaf1_edx(4); }
int minispec_has_cmov(void)       { return minispec_leaf1_edx(15); }
int minispec_has_fcmov(void)      { return minispec_leaf1_edx(16); }
int minispec_has_mmx(void)        { return minispec_leaf1_edx(23); }
int minispec_has_sse(void)        { return minispec_leaf1_edx(25); }
int minispec_has_sse2(void)       { return minispec_leaf1_edx(26); }
int minispec_has_sse3(void)       { return minispec_leaf1_ecx(0); }
int minispec_has_ssse3(void)      { return minispec_leaf1_ecx(9); }
int minispec_has_sse41(void)      { return minispec_leaf1_ecx(19); }
int minispec_has_sse42(void)      { return minispec_leaf1_ecx(20); }
int minispec_has_aes(void)        { return minispec_leaf1_ecx(25); }
int minispec_has_avx(void)        { return minispec_leaf1_ecx(28); }
int minispec_has_cmpxchg16b(void) { return minispec_leaf1_ecx(13); }
int minispec_has_lahf_sahf(void)  { return minispec_leaf_ext_ecx(0); }
int minispec_has_sse4a(void)      { return minispec_leaf_ext_ecx(6); }
int minispec_has_3dnow(void)      { return minispec_leaf_ext_edx(31); }
int minispec_has_avx2(void)       { return minispec_leaf7_ebx(5); }

/* ------------------------------------------------------------
   CPU strings
   ------------------------------------------------------------ */

static char minispec_vendor_str[13];
static char minispec_brand_str[49];
static int minispec_cpu_str_init = 0;

static void minispec_init_cpu_strings(void)
{
#if MINISPEC_X86
    if (minispec_cpu_str_init) return;

    int r[4];

    minispec_cpuid(r, 0);
    memcpy(minispec_vendor_str+0, &r[1], 4);
    memcpy(minispec_vendor_str+4, &r[3], 4);
    memcpy(minispec_vendor_str+8, &r[2], 4);
    minispec_vendor_str[12] = 0;

    for (int i = 0; i < 3; i++) {
        minispec_cpuid(r, 0x80000002 + i);
        memcpy(minispec_brand_str + i*16, r, 16);
    }
    minispec_brand_str[48] = 0;

    minispec_cpu_str_init = 1;
#endif
}

const char* minispec_cpu_vendor(void)
{
#if MINISPEC_X86
    minispec_init_cpu_strings();
    return minispec_vendor_str;
#else
    return "unknown";
#endif
}

const char* minispec_cpu_brand(void)
{
#if MINISPEC_X86
    minispec_init_cpu_strings();
    return minispec_brand_str;
#else
    return "unknown";
#endif
}

/* ------------------------------------------------------------
   CPU cores
   ------------------------------------------------------------ */

uint32_t minispec_cpu_cores(void)
{
#if defined(MINISPEC_WINDOWS)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (uint32_t)info.dwNumberOfProcessors;

#elif defined(MINISPEC_APPLE)
    int count = 0;
    size_t size = sizeof(count);
    sysctlbyname("hw.ncpu", &count, &size, NULL, 0);
    return (uint32_t)count;

#elif defined(MINISPEC_LINUX)
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return (uint32_t)(n > 0 ? n : 1);
#else
    return 1;
#endif
}

/* ------------------------------------------------------------
   Memory
   ------------------------------------------------------------ */

uint64_t minispec_memory_bytes(void)
{
#if defined(MINISPEC_WINDOWS)
    MEMORYSTATUSEX s;
    s.dwLength = sizeof(s);
    GlobalMemoryStatusEx(&s);
    return (uint64_t)s.ullTotalPhys;

#elif defined(MINISPEC_APPLE)
    uint64_t mem = 0;
    size_t size = sizeof(mem);
    sysctlbyname("hw.memsize", &mem, &size, NULL, 0);
    return mem;

#elif defined(MINISPEC_LINUX)
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return (uint64_t)pages * (uint64_t)page_size;
#else
    return 0;
#endif
}

#endif /* MINISPEC_IMPLEMENTATION */
#endif /* MINISPEC_H */
