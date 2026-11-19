/* Minimal Linux shim for the few Win32 names Notepad3's PCRE2RegExEngine.cxx uses:
 * WCHAR, WideCharToMultiByte(CP_UTF8, ...), OutputDebugStringA, ARRAYSIZE, WINAPI. */
#pragma once
#include <stddef.h>
#include <wchar.h>
#ifndef WINAPI
#define WINAPI
#endif
#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
#define CP_UTF8 65001
typedef wchar_t WCHAR;
static inline void OutputDebugStringA(const char *s) { (void)s; }
/* UTF-16 code units -> UTF-8, as Win32 does (lone surrogates become U+FFFD).
 * Only the cchWideChar == -1 (NUL-terminated, NUL included in the count) form is used. */
static inline int WideCharToMultiByte(unsigned cp, unsigned long fl, const WCHAR *w, int cw,
                                      char *out, int cb, const char *d, int *u) {
  (void)cp; (void)fl; (void)d; (void)u;
  int n = 0;
  for (int i = 0; cw < 0 || i < cw; ++i) {
    unsigned c = (unsigned)w[i] & 0xFFFFu;
    unsigned char b[4]; int k;
    if (c >= 0xD800 && c <= 0xDFFF) c = 0xFFFD;
    if (c < 0x80) { b[0] = (unsigned char)c; k = 1; }
    else if (c < 0x800) { b[0] = 0xC0 | (c >> 6); b[1] = 0x80 | (c & 0x3F); k = 2; }
    else { b[0] = 0xE0 | (c >> 12); b[1] = 0x80 | ((c >> 6) & 0x3F); b[2] = 0x80 | (c & 0x3F); k = 3; }
    if (n + k > cb) return 0;
    for (int j = 0; j < k; ++j) out[n++] = (char)b[j];
    if (c == 0) break;
  }
  return n;
}
#include <ctype.h>
/* Win32 IsCharAlphaNumericA (ANSI code page); the C locale's isalnum is the portable equivalent. */
static inline int IsCharAlphaNumericA(char ch) { return isalnum((unsigned char)ch) != 0; }
