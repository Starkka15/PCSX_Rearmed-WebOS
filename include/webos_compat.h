/*
 * WebOS compatibility header
 * Provides fallbacks for missing builtins
 */

#ifndef __WEBOS_COMPAT_H__
#define __WEBOS_COMPAT_H__

/* Skip C code when included from assembly files */
#ifndef __ASSEMBLER__

/*
 * GCC 4.9 for ARM doesn't have __builtin_bswap16.
 * We can't use #ifndef to detect builtins, so we define our own function
 * and use a macro to redirect all calls to it.
 */
static inline __attribute__((always_inline)) unsigned short __webos_bswap16(unsigned short x) {
    return (unsigned short)((x >> 8) | (x << 8));
}
#define __builtin_bswap16(x) __webos_bswap16(x)

#endif /* __ASSEMBLER__ */

#endif /* __WEBOS_COMPAT_H__ */
