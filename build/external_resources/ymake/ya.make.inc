IF (HOST_OS_DARWIN AND HOST_ARCH_X86_64 OR HOST_OS_DARWIN AND HOST_ARCH_ARM64 OR HOST_OS_LINUX AND HOST_ARCH_PPC64LE OR HOST_OS_LINUX AND HOST_ARCH_X86_64 OR HOST_OS_WINDOWS AND HOST_ARCH_X86_64)
ELSE()
    MESSAGE(FATAL_ERROR Unsupported host platform for YMAKE)
ENDIF()

DECLARE_EXTERNAL_HOST_RESOURCES_BUNDLE(
    YMAKE
    sbr:2807634670 FOR DARWIN
    sbr:2807635068 FOR DARWIN-ARM64
    sbr:2807634419 FOR LINUX-PPC64LE
    sbr:2807634842 FOR LINUX
    sbr:2807634225 FOR WIN32
)