# Generated by devtools/yamaker.

LIBRARY()

OWNER(
    orivej
    g:cpp-contrib
)

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/llvm12
    contrib/libs/llvm12/lib/DebugInfo/DWARF
    contrib/libs/llvm12/lib/DebugInfo/PDB
    contrib/libs/llvm12/lib/Demangle
    contrib/libs/llvm12/lib/Object
    contrib/libs/llvm12/lib/Support
)

ADDINCL(
    contrib/libs/llvm12/lib/DebugInfo/Symbolize
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    DIPrinter.cpp
    SymbolizableObjectFile.cpp
    Symbolize.cpp
)

END()