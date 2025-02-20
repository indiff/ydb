# Generated by devtools/yamaker from nixpkgs 22.05.

LIBRARY()

LICENSE(BSL-1.0)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

OWNER(
    g:cpp-contrib
    g:taxi-common
)

VERSION(1.81.0)

ORIGINAL_SOURCE(https://github.com/boostorg/ptr_container/archive/boost-1.81.0.tar.gz)

PEERDIR(
    contrib/restricted/boost/array
    contrib/restricted/boost/assert
    contrib/restricted/boost/circular_buffer
    contrib/restricted/boost/config
    contrib/restricted/boost/core
    contrib/restricted/boost/iterator
    contrib/restricted/boost/mpl
    contrib/restricted/boost/range
    contrib/restricted/boost/serialization
    contrib/restricted/boost/smart_ptr
    contrib/restricted/boost/static_assert
    contrib/restricted/boost/type_traits
    contrib/restricted/boost/unordered
    contrib/restricted/boost/utility
)

ADDINCL(
    GLOBAL contrib/restricted/boost/ptr_container/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

END()
