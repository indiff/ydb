# Generated by devtools/yamaker from nixpkgs 22.05.

LIBRARY()

LICENSE(BSL-1.0)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

OWNER(
    g:cpp-contrib
    g:taxi-common
)

VERSION(1.80.0)

ORIGINAL_SOURCE(https://github.com/boostorg/callable_traits/archive/boost-1.80.0.tar.gz)

ADDINCL(
    GLOBAL contrib/restricted/boost/callable_traits/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

END()
