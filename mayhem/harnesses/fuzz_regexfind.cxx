// Notepad3's document-less regex search: RegExFind() (SimplePCRE2Engine) exported from
// scintilla/pcre2/scintilla/PCRE2RegExEngine.cxx, driven the way its only upstream caller does
// (src/Edit.c, EditURLDecode): the fixed hyperlink pattern
// HYPLNK_REGEX_FULL, case-insensitive, over a NUL-terminated UTF-8 subject.
// HYPLNK_REGEX_FULL is extracted from src/Edit.c at build time into hyplnk_regex.h
// (mayhem/build.sh), so the harness always uses upstream's current pattern.
// Input: the subject text (whole input).
#include <cstddef>
#include <cstdint>
#include <string>

#include "hyplnk_regex.h"

extern "C" ptrdiff_t RegExFind(const char *pchPattern, const char *pchText, const bool caseSensitive, int *matchLen_out);

// Strict UTF-8 check (no overlongs, no surrogates, <= U+10FFFF), the set PCRE2's own validator accepts.
static bool valid_utf8(const uint8_t *s, size_t n) {
  size_t i = 0;
  while (i < n) {
    uint8_t c = s[i];
    if (c < 0x80) { ++i; continue; }
    size_t len; uint32_t cp;
    if (c >= 0xC2 && c <= 0xDF) { len = 2; cp = c & 0x1F; }
    else if (c >= 0xE0 && c <= 0xEF) { len = 3; cp = c & 0x0F; }
    else if (c >= 0xF0 && c <= 0xF4) { len = 4; cp = c & 0x07; }
    else return false;
    if (n - i < len) return false;
    for (size_t k = 1; k < len; ++k) {
      if ((s[i + k] & 0xC0) != 0x80) return false;
      cp = (cp << 6) | (s[i + k] & 0x3F);
    }
    if ((len == 3 && cp < 0x800) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)) ||
        (cp >= 0xD800 && cp <= 0xDFFF)) return false;
    i += len;
  }
  return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Precondition from the library's contract: SimplePCRE2Engine::Find compiles with PCRE2_UTF and
  // matches with PCRE2_NO_UTF_CHECK (PCRE2RegExEngine.cxx), which per pcre2api requires a valid
  // UTF-8 subject. The upstream caller guarantees it: the subject is WideCharToMultiByte(CP_UTF8)
  // output (src/Edit.c, just before the RegExFind call). Reject anything else.
  if (!valid_utf8(data, size)) return -1;
  std::string text(reinterpret_cast<const char *>(data), size);  // C string: stops at the first NUL, as upstream's
  int mlen = 0;
  RegExFind(HYPLNK_REGEX_FULL, text.c_str(), false, &mlen);
  return 0;
}
