// Notepad3's (patched) uchardet encoding detector, run on every opened file.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "uchardet.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  uchardet_t ud = uchardet_new();
  if (!ud) return 0;
  // Feed in two chunks so the probers' cross-buffer state is exercised.
  size_t half = size / 2;
  uchardet_handle_data(ud, reinterpret_cast<const char *>(data), half);
  uchardet_handle_data(ud, reinterpret_cast<const char *>(data) + half, size - half);
  uchardet_data_end(ud);
  const char *cs = uchardet_get_charset(ud);
  if (cs) { volatile size_t n = strlen(cs); (void)n; }
  volatile float c = uchardet_get_confidence(ud); (void)c;
  uchardet_delete(ud);
  return 0;
}
