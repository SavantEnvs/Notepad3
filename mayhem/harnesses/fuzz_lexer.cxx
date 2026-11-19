// Notepad3 syntax highlighting: Lexilla CreateLexer(name) -> ILexer5::Lex + Fold over
// file content, for every lexer in Notepad3's (patched) Lexilla catalogue.
// Input: [lexer selector byte][flags byte] document-text
// The document is Scintilla's own Document (it implements IDocument), as in the editor.
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
#include "Lexilla.h"

using namespace Scintilla;
using namespace Scintilla::Internal;

static const char *kWords =
    "if else while for do return function class var let const def end then begin "
    "true false null int char void public private static import include define";

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 2) return 0;
  const int count = GetLexerCount();
  if (count <= 0) return 0;
  char name[128] = {0};
  GetLexerName(static_cast<unsigned>(data[0] % count), name, sizeof(name));
  ILexer5 *lexer = CreateLexer(name);
  if (!lexer) return 0;
  const uint8_t fl = data[1];
  lexer->PropertySet("fold", "1");
  lexer->PropertySet("fold.compact", (fl & 2) ? "1" : "0");
  lexer->PropertySet("fold.comment", "1");
  lexer->PropertySet("fold.preprocessor", "1");
  lexer->PropertySet("fold.html", "1");
  lexer->PropertySet("lexer.cpp.track.preprocessor", (fl & 4) ? "1" : "0");
  // One keyword list per set the lexer describes (newline-separated names).
  const char *sets = lexer->DescribeWordListSets();
  int nsets = 0;
  if (sets && *sets) {
    nsets = 1;
    for (const char *p = sets; *p; ++p) if (*p == '\n') ++nsets;
  }
  for (int i = 0; i < nsets; ++i) lexer->WordListSet(i, kWords);

  Document doc(DocumentOption::Default);
  if (fl & 1) doc.SetDBCSCodePage(SC_CP_UTF8);
  doc.InsertString(0, reinterpret_cast<const char *>(data + 2), static_cast<Sci::Position>(size - 2));
  const Sci::Position len = doc.Length();
  IDocument *pdoc = &doc;
  doc.StartStyling(0);
  lexer->Lex(0, len, 0, pdoc);
  lexer->Fold(0, len, 0, pdoc);
  lexer->Release();
  return 0;
}
