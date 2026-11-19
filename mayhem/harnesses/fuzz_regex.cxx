// Notepad3 find/replace: Scintilla Document::FindText(RegExp) + SubstituteByPosition,
// served by Notepad3's PCRE2RegExEngine (scintilla/pcre2/scintilla/PCRE2RegExEngine.cxx).
// Input: [flags byte: 1 MatchCase, 2 WholeWord, 4 WordStart, 8 DotMatchAll] pattern \0 replacement \0 document-text
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <algorithm>

#include "ScintillaTypes.h"
#include "ILoader.h"
#include "ILexer.h"
#include "Scintilla.h"
#include "Debugging.h"
#include "Geometry.h"
#include "Platform.h"
#include "CharacterCategoryMap.h"
#include "Position.h"
#include "SplitVector.h"
#include "Partitioning.h"
#include "RunStyles.h"
#include "CellBuffer.h"
#include "CharClassify.h"
#include "Decoration.h"
#include "CaseFolder.h"
#include "Document.h"

using namespace Scintilla;
using namespace Scintilla::Internal;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 1) return 0;
  const uint8_t fl = data[0];
  std::string rest(reinterpret_cast<const char *>(data + 1), size - 1);
  size_t p1 = rest.find('\0');
  if (p1 == std::string::npos) return 0;
  size_t p2 = rest.find('\0', p1 + 1);
  if (p2 == std::string::npos) return 0;
  std::string pattern = rest.substr(0, p1);
  std::string repl = rest.substr(p1 + 1, p2 - p1 - 1);
  std::string text = rest.substr(p2 + 1);

  Document doc(DocumentOption::Default);
  // Notepad3 always runs Scintilla in UTF-8 (src/Notepad3.c: SciCall_SetCodePage(SC_CP_UTF8)).
  // The document bytes stay raw: EditLoadFile puts a UTF-8-BOM file's bytes into the buffer
  // verbatim with no validity check (src/Edit.c, `if (encDetection.bIsUTF8Sig) EditSetNewText(...)`),
  // as it does for files >2GB, so invalid UTF-8 in the document is reachable in the app.
  doc.SetDBCSCodePage(SC_CP_UTF8);
  doc.InsertString(0, text.data(), static_cast<Sci::Position>(text.size()));
  FindOption opts = FindOption::RegExp;
  if (fl & 0x01) opts = opts | FindOption::MatchCase;
  if (fl & 0x02) opts = opts | FindOption::WholeWord;
  if (fl & 0x04) opts = opts | FindOption::WordStart;
  if (fl & 0x08) opts = opts | FindOption::DotMatchAll;
  const Sci::Position len = doc.Length();
  // Search forward from the start, then backward from the end (Notepad3's find-previous).
  // An invalid pattern raises RegexError, which Scintilla's Editor::FindText catches and turns
  // into SC_STATUS_WARN_REGEX; the harness does the same.
  try {
  for (int dir = 0; dir < 2; ++dir) {
    Sci::Position mlen = static_cast<Sci::Position>(pattern.size());
    Sci::Position pos = dir == 0 ? doc.FindText(0, len, pattern.c_str(), opts, &mlen)
                                 : doc.FindText(len, 0, pattern.c_str(), opts, &mlen);
    if (pos >= 0) {
      Sci::Position rlen = static_cast<Sci::Position>(repl.size());
      const char *out = doc.SubstituteByPosition(repl.c_str(), &rlen);
      if (out && rlen > 0) { volatile char c = out[rlen - 1]; (void)c; }
    }
  }
  } catch (const RegexError &) {
  }
  return 0;
}
