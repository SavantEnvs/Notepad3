#!/usr/bin/env bash
#
# mayhem/build.sh — Notepad3 fuzz harnesses + known-answer self test.
#
# Notepad3 is a Win32 app built with MSBuild; there is no Linux build. Its portable engines are
# compiled here straight from the upstream source dirs (no upstream file is modified; the few
# Win32 names PCRE2RegExEngine.cxx needs come from mayhem/shim/):
#   scintilla/src (Document core) + scintilla/pcre2 (NP3 PCRE2RegExEngine glue + PCRE2 10.4x)
#   lexilla (lexlib + lexers + NP3 lexers_x), src/uchardet, src/tinyexprcpp.
# Objects go to mayhem/build/ (gitignored). Every run rebuilds from scratch (idempotent, offline).
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
B="$SRC/mayhem/build"
rm -rf "$B"
mkdir -p "$B"

SCI=scintilla/src
SCI_SRCS="CaseConvert CaseFolder CellBuffer ChangeHistory CharacterCategoryMap CharacterType
  CharClassify DBCS Decoration Document PerLine RESearch RunStyles UndoHistory UniConversion UniqueString"
PCRE2_SRCS="auto_possess chartables chkdint compile compile_cgroup compile_class config context
  convert dfa_match error extuni find_bracket jit_compile maketables match match_data newline
  match_next ord2utf pattern_info script_run serialize string_utils study substitute substring tables ucd
  valid_utf xclass"

# Shared preprocessor flags (as Scintilla.vcxproj / Lexilla.vcxproj define them).
DEFS=(-D'__declspec(x)=' -DNP3 -DNO_CXX11_REGEX -DSCI_OWNREGEX -DPCRE2_CODE_UNIT_WIDTH=8
      -DPCRE2_STATIC -D_stricmp=strcasecmp -D_strnicmp=strncasecmp -D_strdup=strdup)
INC=(-I"$SRC/mayhem/shim" -I"$SRC/scintilla/include" -I"$SRC/scintilla/src"
     -I"$SRC/lexilla/include" -I"$SRC/lexilla/lexlib" -I"$SRC/lexilla/lexers_x"
     -I"$SRC/src/uchardet/uchardet/src" -I"$SRC/src/tinyexprcpp")
CXXSTD=(-std=c++20 -include algorithm -include cstring -include strings.h -Wno-everything)
CSTD=(-std=c17 -Wno-everything -include "$SRC/mayhem/shim/pcre2_config_nojit.h")

# Source list: "<lang> <path>" per line.
list_sources() {
  for f in $SCI_SRCS; do echo "cxx $SCI/$f.cxx"; done
  echo "cxx scintilla/pcre2/scintilla/PCRE2RegExEngine.cxx"
  for f in $PCRE2_SRCS; do echo "c scintilla/pcre2/src/pcre2_$f.c"; done
  for f in lexilla/lexlib/*.cxx lexilla/lexers/*.cxx lexilla/lexers_x/*.cxx; do echo "cxx $f"; done
  echo "cxx lexilla/src/Lexilla.cxx"
  for f in src/uchardet/uchardet/src/*.cpp src/uchardet/uchardet/src/LangModels/*.cpp; do echo "cxx $f"; done
  echo "cxx src/tinyexprcpp/tinyexpr.cpp"
  echo "cxx src/tinyexprcpp/tinyexpr_cif.cpp"
  echo "cxx mayhem/harnesses/sci_platform.cxx"
}

# compile_all <objdir> <extra flags...>: compile every source into <objdir>/<path>.o in parallel.
compile_all() {
  local out="$1"; shift
  export CB_OUT="$out" CB_FLAGS="$*" CB_DEFS="${DEFS[*]@Q}" CB_INC="${INC[*]@Q}" \
         CB_CXXSTD="${CXXSTD[*]}" CB_CSTD="${CSTD[*]}"
  list_sources | xargs -P"$MAYHEM_JOBS" -L1 bash -c '
    set -e; lang="$0"; src="$1"; o="$CB_OUT/${src%.*}.o"; mkdir -p "$(dirname "$o")"
    eval "defs=($CB_DEFS)"; eval "inc=($CB_INC)"
    if [ "$lang" = c ]; then $CC $CB_CSTD "${defs[@]}" "${inc[@]}" $CB_FLAGS -c "$src" -o "$o"
    else $CXX $CB_CXXSTD "${defs[@]}" "${inc[@]}" $CB_FLAGS -c "$src" -o "$o"; fi'
  find "$out" -name '*.o' | LC_ALL=C sort > "$out/objs.txt"
}

# 1) Project objects with sanitizers + SanCov (fuzzer-no-link) + DWARF 3.
FUZZ_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link"
compile_all "$B/fuzz" $FUZZ_FLAGS
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$B/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"
mapfile -t FOBJS < "$B/fuzz/objs.txt"

# fuzz_regexfind uses upstream's own hyperlink pattern (the one RegExFind's only caller,
# EditURLDecode in src/Edit.c, passes): copy its #defines out of src/Edit.c (CRLF stripped).
awk '{ sub(/\r$/, "") }
     /^#define HYPLNK_REGEX_VALID_CDPT / { print }
     /^#define HYPLNK_REGEX_FULL / { f = 1 }
     f { print; if ($0 !~ /\\$/) f = 0 }' src/Edit.c > "$B/hyplnk_regex.h"
grep -q '^#define HYPLNK_REGEX_FULL ' "$B/hyplnk_regex.h" \
  || { echo "build.sh: HYPLNK_REGEX_FULL not found in src/Edit.c" >&2; exit 1; }

# 2) Harnesses: libFuzzer binary + run-once standalone reproducer each.
for h in fuzz_regex fuzz_regexfind fuzz_uchardet fuzz_tinyexpr fuzz_lexer; do
  if [ -f "mayhem/harnesses/$h.c" ]; then
    $CC "${CSTD[@]}" "${INC[@]}" $FUZZ_FLAGS -c "mayhem/harnesses/$h.c" -o "$B/$h.o"
  else
    $CXX "${CXXSTD[@]}" "${DEFS[@]}" "${INC[@]}" -I"$B" $FUZZ_FLAGS -c "mayhem/harnesses/$h.cxx" -o "$B/$h.o"
  fi
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$B/$h.o" "${FOBJS[@]}" "$B/lsan_off.o" \
      -lm -o "$SRC/$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$B/standalone_main.o" "$B/$h.o" "${FOBJS[@]}" "$B/lsan_off.o" \
      -lm -o "$SRC/$h-standalone"
done

# 3) Known-answer self test with normal flags (no sanitizers) for mayhem/test.sh.
compile_all "$B/test" -O1 $COVERAGE_FLAGS
mapfile -t TOBJS < "$B/test/objs.txt"
$CXX "${CXXSTD[@]}" "${DEFS[@]}" "${INC[@]}" -O1 $COVERAGE_FLAGS -c mayhem/harnesses/np3_selftest.cxx \
    -o "$B/np3_selftest.o"
$CXX -O1 $COVERAGE_FLAGS "$B/np3_selftest.o" "${TOBJS[@]}" -lm -o "$B/np3_selftest"
echo "build.sh: done"
