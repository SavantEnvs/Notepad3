/* Upstream's Windows-flavoured scintilla/pcre2/src/config.h, minus the JIT: JIT-compiled
 * matchers are machine code that carries no ASan/UBSan/SanCov instrumentation, so the fuzzer
 * would be blind inside them. With SUPPORT_JIT unset, pcre2_jit_compile() returns an error,
 * which PCRE2RegExEngine ignores, and every match runs in the instrumented interpreter. */
#include "../../scintilla/pcre2/src/config.h"
#undef SUPPORT_JIT
#undef HAVE_WINDOWS_H
