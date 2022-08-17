LIBRARY()

WITHOUT_LICENSE_TEXTS()

LICENSE(Apache-2.0)

OWNER(
    ayles
    prime
    g:cpp-contrib
)

SRCDIR(contrib/libs/tcmalloc)

INCLUDE(../common.inc)

GLOBAL_SRCS(
    # Options
    tcmalloc/want_hpaa.cc
    tcmalloc/want_numa_aware.cc
)

CFLAGS(
    -DTCMALLOC_256K_PAGES
    -DTCMALLOC_NUMA_AWARE
)

END()