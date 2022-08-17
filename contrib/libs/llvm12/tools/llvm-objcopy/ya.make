# Generated by devtools/yamaker.

PROGRAM()

OWNER(
    orivej
    g:cpp-contrib
)

LICENSE(Apache-2.0 WITH LLVM-exception)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/llvm12
    contrib/libs/llvm12/include
    contrib/libs/llvm12/lib/BinaryFormat
    contrib/libs/llvm12/lib/Bitcode/Reader
    contrib/libs/llvm12/lib/Bitstream/Reader
    contrib/libs/llvm12/lib/Demangle
    contrib/libs/llvm12/lib/IR
    contrib/libs/llvm12/lib/MC
    contrib/libs/llvm12/lib/MC/MCParser
    contrib/libs/llvm12/lib/Object
    contrib/libs/llvm12/lib/Option
    contrib/libs/llvm12/lib/Remarks
    contrib/libs/llvm12/lib/Support
    contrib/libs/llvm12/lib/TextAPI/MachO
)

ADDINCL(
    ${ARCADIA_BUILD_ROOT}/contrib/libs/llvm12/tools/llvm-objcopy
    contrib/libs/llvm12/tools/llvm-objcopy
)

NO_COMPILER_WARNINGS()

NO_UTIL()

SRCS(
    Buffer.cpp
    COFF/COFFObjcopy.cpp
    COFF/Object.cpp
    COFF/Reader.cpp
    COFF/Writer.cpp
    CopyConfig.cpp
    ELF/ELFConfig.cpp
    ELF/ELFObjcopy.cpp
    ELF/Object.cpp
    MachO/MachOLayoutBuilder.cpp
    MachO/MachOObjcopy.cpp
    MachO/MachOReader.cpp
    MachO/MachOWriter.cpp
    MachO/Object.cpp
    llvm-objcopy.cpp
    wasm/Object.cpp
    wasm/Reader.cpp
    wasm/WasmObjcopy.cpp
    wasm/Writer.cpp
)

END()