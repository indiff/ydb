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
    contrib/libs/llvm12/lib/BinaryFormat
    contrib/libs/llvm12/lib/MC
    contrib/libs/llvm12/lib/Object
    contrib/libs/llvm12/lib/Support
)

ADDINCL(
    contrib/libs/llvm12/lib/DebugInfo/DWARF
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    DWARFAbbreviationDeclaration.cpp
    DWARFAcceleratorTable.cpp
    DWARFAddressRange.cpp
    DWARFCompileUnit.cpp
    DWARFContext.cpp
    DWARFDataExtractor.cpp
    DWARFDebugAbbrev.cpp
    DWARFDebugAddr.cpp
    DWARFDebugArangeSet.cpp
    DWARFDebugAranges.cpp
    DWARFDebugFrame.cpp
    DWARFDebugInfoEntry.cpp
    DWARFDebugLine.cpp
    DWARFDebugLoc.cpp
    DWARFDebugMacro.cpp
    DWARFDebugPubTable.cpp
    DWARFDebugRangeList.cpp
    DWARFDebugRnglists.cpp
    DWARFDie.cpp
    DWARFExpression.cpp
    DWARFFormValue.cpp
    DWARFGdbIndex.cpp
    DWARFListTable.cpp
    DWARFLocationExpression.cpp
    DWARFTypeUnit.cpp
    DWARFUnit.cpp
    DWARFUnitIndex.cpp
    DWARFVerifier.cpp
)

END()