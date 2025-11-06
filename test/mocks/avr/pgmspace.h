/**
 * @file avr/pgmspace.h mock
 * @brief Mock AVR PROGMEM header for native tests
 */

#ifndef __PGMSPACE_H_
#define __PGMSPACE_H_ 1

#include <stdint.h>
#include <string.h>

#define PROGMEM
#define PGM_P const char *
#define PGM_VOID_P const void *

#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#define pgm_read_float(addr) (*(const float *)(addr))
#define pgm_read_ptr(addr) (*(void * const *)(addr))
#define pgm_read_byte_near(addr) pgm_read_byte(addr)
#define pgm_read_word_near(addr) pgm_read_word(addr)
#define pgm_read_dword_near(addr) pgm_read_dword(addr)
#define pgm_read_float_near(addr) pgm_read_float(addr)
#define pgm_read_ptr_near(addr) pgm_read_ptr(addr)

#define PSTR(s) (s)
#define FPSTR(s) (s)

#define _SFR_BYTE(n) (n)

#ifdef __cplusplus
extern "C" {
#endif

static inline char * strcpy_P(char *dest, const char *src) {
    return strcpy(dest, src);
}

static inline int strcmp_P(const char *s1, const char *src) {
    return strcmp(s1, src);
}

static inline size_t strlen_P(const char *s) {
    return strlen(s);
}

static inline void * memcpy_P(void *dest, const void *src, size_t n) {
    return memcpy(dest, src, n);
}

#ifdef __cplusplus
}
#endif

#endif /* __PGMSPACE_H_ */
