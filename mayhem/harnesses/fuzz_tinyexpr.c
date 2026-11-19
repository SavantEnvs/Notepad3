/* Notepad3's expression evaluator (status bar "evaluate selection"): TinyExpr++ through
 * Notepad3's C interface src/tinyexprcpp/tinyexpr_cif.cpp. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "tinyexpr_cif.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char *s = (char *)malloc(size + 1);
  if (!s) return 0;
  memcpy(s, data, size);
  s[size] = '\0';
  te_int_t err = 0;
  int isLog = 0;
  volatile double r = te_interp_ex(s, &err, &isLog);
  (void)r;
  const char *str = te_interp_str(s, &err);
  if (str) { volatile size_t n = strlen(str); (void)n; }
  (void)te_is_logical_expr(s);
  free(s);
  return 0;
}
