LIBRARY()

BUILD_ONLY_IF(FATAL_ERROR ARCH_TYPE_64)

NO_UTIL()

NO_COMPILER_WARNINGS()

SRCS(
    lf_allocX64.cpp
)

PEERDIR(
    library/cpp/malloc/api
)

SET(IDE_FOLDER "util")

END()
