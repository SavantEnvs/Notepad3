// Known-answer tests for the Notepad3 engines the fuzz targets exercise. Prints one
// "PASS <name>" / "FAIL <name> ..." line per check; mayhem/test.sh counts them.
#include <cmath>
#include <cstdio>
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
#include "SciLexer.h"
#include "uchardet.h"
#include "tinyexpr_cif.h"

using namespace Scintilla;
using namespace Scintilla::Internal;

extern "C" ptrdiff_t RegExFind(const char *pchPattern, const char *pchText, const bool caseSensitive, int *matchLen_out);

static int fails = 0;
static void check(const char *name, bool ok, const std::string &detail = "") {
  if (ok) printf("PASS %s\n", name);
  else { printf("FAIL %s %s\n", name, detail.c_str()); ++fails; }
}

static std::string detect(const std::string &s) {
  uchardet_t ud = uchardet_new();
  uchardet_handle_data(ud, s.data(), s.size());
  uchardet_data_end(ud);
  std::string r = uchardet_get_charset(ud);
  uchardet_delete(ud);
  return r;
}

int main() {
  // --- TinyExpr++ via Notepad3's C interface
  te_int_t err = -1;
  double v = te_interp("2+3*4", &err);
  check("tinyexpr_arith", err == 0 && v == 14.0, std::to_string(v));
  v = te_interp("sqrt(16)+2^3", &err);
  check("tinyexpr_funcs", err == 0 && v == 12.0, std::to_string(v));
  int isLog = -1;
  v = te_interp_ex("3 > 2", &err, &isLog);
  check("tinyexpr_logical", err == 0 && v == 1.0 && isLog == 1, std::to_string(v));
  const char *s = te_interp_str("1 == 2", &err);
  check("tinyexpr_str_false", s && std::string(s) == "false", s ? s : "(null)");
  s = te_interp_str("7/2", &err);
  check("tinyexpr_str_num", s && std::string(s) == "3.5", s ? s : "(null)");
  te_interp("(1+", &err);
  check("tinyexpr_error", err != 0, std::to_string((long long)err));

  // --- uchardet
  std::string u8 = "Notepad3 \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x81\xae\xe3\x83\x86\xe3\x82\xad"
                   "\xe3\x82\xb9\xe3\x83\x88 \xc3\xa9\xc3\xa8\xc3\xa0 \xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2"
                   "\xd0\xb5\xd1\x82 \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";
  std::string cs = detect(u8);
  check("uchardet_utf8", cs == "UTF-8", cs);
  std::string sjis = "\x93\xfa\x96\x7b\x8c\xea\x82\xcc\x83\x65\x83\x4c\x83\x58\x83\x67\x82\xc5\x82\xb7"
                     "\x81\x42\x82\xb1\x82\xea\x82\xcd\x83\x65\x83\x58\x83\x67\x82\xc5\x82\xb7\x81\x42"
                     "\x93\xfa\x96\x7b\x8c\xea\x82\xcc\x95\xb6\x8f\xcd\x82\xc5\x82\xb7\x81\x42";
  cs = detect(sjis);
  check("uchardet_sjis", cs == "SHIFT_JIS", cs);

  // --- Scintilla Document + Notepad3 PCRE2RegExEngine
  {
    Document doc(DocumentOption::Default);
    std::string text = "foo bar baz123 qux\nline two baz9\n";
    doc.InsertString(0, text.data(), text.size());
    Sci::Position len = 0;
    std::string pat = "ba(z)(\\d+)";
    len = pat.size();
    Sci::Position pos = doc.FindText(0, doc.Length(), pat.c_str(), FindOption::RegExp | FindOption::MatchCase, &len);
    check("regex_find", pos == 8 && len == 6, std::to_string(pos) + "/" + std::to_string(len));
    Sci::Position rl = 7;  // in: length of the replacement text, out: result length
    const char *rep = doc.SubstituteByPosition("<$2:$1>", &rl);
    check("regex_subst", rep && std::string(rep, rl) == "<123:z>", rep ? std::string(rep, rl) : "(null)");
    std::string pat4 = "Q(U)X";
    len = pat4.size();
    pos = doc.FindText(0, doc.Length(), pat4.c_str(), FindOption::RegExp, &len);
    check("regex_caseless", pos == 15 && len == 3, std::to_string(pos) + "/" + std::to_string(len));
    std::string pat2 = "^line";
    len = pat2.size();
    pos = doc.FindText(0, doc.Length(), pat2.c_str(), FindOption::RegExp, &len);
    check("regex_linestart", pos == 19 && len == 4, std::to_string(pos) + "/" + std::to_string(len));
    std::string pat3 = "QUX";
    len = pat3.size();
    pos = doc.FindText(0, doc.Length(), pat3.c_str(), FindOption::RegExp | FindOption::MatchCase, &len);
    check("regex_matchcase_miss", pos == -1, std::to_string(pos));
  }
  int ml = 0;
  ptrdiff_t rp = RegExFind("b+c", "aabbbcd", true, &ml);
  check("regexfind", rp == 2 && ml == 4, std::to_string(rp) + "/" + std::to_string(ml));

  // --- Lexilla CPP lexer over a Scintilla Document
  {
    ILexer5 *lx = CreateLexer("cpp");
    check("lexer_create", lx != nullptr);
    if (lx) {
      lx->WordListSet(0, "int return");
      Document doc(DocumentOption::Default);
      std::string text = "int x = 42; // note\n\"str\"\n";
      doc.InsertString(0, text.data(), text.size());
      doc.StartStyling(0);
      lx->Lex(0, doc.Length(), 0, &doc);
      check("lexer_keyword", doc.StyleAt(0) == SCE_C_WORD, std::to_string(doc.StyleAt(0)));
      check("lexer_number", doc.StyleAt(8) == SCE_C_NUMBER, std::to_string(doc.StyleAt(8)));
      check("lexer_comment", doc.StyleAt(13) == SCE_C_COMMENTLINE, std::to_string(doc.StyleAt(13)));
      check("lexer_string", doc.StyleAt(21) == SCE_C_STRING, std::to_string(doc.StyleAt(21)));
      lx->Release();
    }
  }
  printf("selftest: %d failure(s)\n", fails);
  return fails ? 1 : 0;
}
