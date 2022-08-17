RESOURCES_LIBRARY()

OWNER(g:yatool)

IF (HOST_OS_DARWIN AND HOST_ARCH_ARM64 OR
    HOST_OS_DARWIN AND HOST_ARCH_X86_64 OR
    HOST_OS_LINUX AND HOST_ARCH_PPC64LE OR
    HOST_OS_LINUX AND HOST_ARCH_X86_64 OR
    HOST_OS_WINDOWS AND HOST_ARCH_X86_64)
ELSE()
    MESSAGE(FATAL_ERROR Unsupported host platform for FLAKE8_PY2)
ENDIF()

DECLARE_EXTERNAL_HOST_RESOURCES_BUNDLE(
    FLAKE8_PY2
    sbr:2488549842 FOR DARWIN-ARM64
    sbr:2488553184 FOR DARWIN
    sbr:2488550572 FOR LINUX-PPC64LE
    sbr:2488555532 FOR LINUX
    sbr:2488554786 FOR WIN32
)

END()