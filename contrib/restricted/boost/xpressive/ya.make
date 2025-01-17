# Generated by devtools/yamaker from nixpkgs 22.05.

LIBRARY()

LICENSE(BSL-1.0)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

OWNER(
    g:cpp-contrib
    g:taxi-common
)

VERSION(1.81.0)

ORIGINAL_SOURCE(https://github.com/boostorg/xpressive/archive/boost-1.81.0.tar.gz)

PEERDIR(
    contrib/restricted/boost/assert
    contrib/restricted/boost/config
    contrib/restricted/boost/conversion
    contrib/restricted/boost/core
    contrib/restricted/boost/exception
    contrib/restricted/boost/fusion
    contrib/restricted/boost/integer
    contrib/restricted/boost/iterator
    contrib/restricted/boost/lexical_cast
    contrib/restricted/boost/mpl
    contrib/restricted/boost/numeric_conversion
    contrib/restricted/boost/optional
    contrib/restricted/boost/preprocessor
    contrib/restricted/boost/proto
    contrib/restricted/boost/range
    contrib/restricted/boost/smart_ptr
    contrib/restricted/boost/static_assert
    contrib/restricted/boost/throw_exception
    contrib/restricted/boost/type_traits
    contrib/restricted/boost/typeof
    contrib/restricted/boost/utility
)

ADDINCL(
    GLOBAL contrib/restricted/boost/xpressive/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

END()
