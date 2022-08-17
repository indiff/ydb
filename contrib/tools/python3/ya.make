PROGRAM()

OWNER(g:contrib orivej)

VERSION(3.9.10)

ORIGINAL_SOURCE(https://github.com/python/cpython)

LICENSE(Python-2.0)

USE_PYTHON3()

PEERDIR(
    contrib/tools/python3/src/Modules/_sqlite
)

CFLAGS(
    -DPy_BUILD_CORE
)

SRCS(
    src/Programs/python.c
)

END()