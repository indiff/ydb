# Generated by devtools/yamaker.

LIBRARY()

WITHOUT_LICENSE_TEXTS()

OWNER(g:cpp-contrib)

LICENSE(Apache-2.0)

PEERDIR(
    contrib/restricted/abseil-cpp/absl/random/internal/randen_detect
    contrib/restricted/abseil-cpp/absl/random/internal/randen_hwaes
    contrib/restricted/abseil-cpp/absl/random/internal/randen_round_keys
    contrib/restricted/abseil-cpp/absl/random/internal/randen_slow
)

ADDINCL(
    GLOBAL contrib/restricted/abseil-cpp
)

NO_COMPILER_WARNINGS()

NO_UTIL()

CFLAGS(
    -DNOMINMAX
)

SRCDIR(contrib/restricted/abseil-cpp/absl/random/internal)

SRCS(
    randen.cc
)

END()