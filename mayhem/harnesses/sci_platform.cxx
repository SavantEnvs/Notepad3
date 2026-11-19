// Linux stand-in for the two Scintilla::Internal::Platform functions the Document core uses;
// the real ones live in scintilla/win32/PlatWin.cxx (Win32-only). Assert behaves like
// Scintilla's other platform layers: report and abort.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include "ScintillaTypes.h"
#include "Debugging.h"
#include "Geometry.h"
#include "Platform.h"

namespace Scintilla::Internal {
void Platform::DebugPrintf(const char *, ...) noexcept {}
void Platform::Assert(const char *c, const char *file, int line) noexcept {
  fprintf(stderr, "Assertion [%s] failed at %s %d\n", c, file, line);
  abort();
}
}
