# Generated by devtools/yamaker from nixpkgs d78cb9fc184bc9f4e3d046ed0175734950fd0342.

LIBRARY()

OWNER(
    dldmitry
    efmv
    kikht
    g:cpp-contrib
)

VERSION(2.1.12)

ORIGINAL_SOURCE(https://github.com/libevent/libevent/releases/download/release-2.1.12-stable/libevent-2.1.12-stable.tar.gz)

LICENSE(
    BSD-3-Clause AND
    CC-PDDC AND
    ISC AND
    MIT
)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

PEERDIR(
    contrib/libs/libevent/event_core
    contrib/libs/libevent/event_extra
    contrib/libs/libevent/event_openssl
    contrib/libs/libevent/event_thread
)

ADDINCL(
    GLOBAL contrib/libs/libevent/include
    contrib/libs/libevent
)

NO_COMPILER_WARNINGS()

NO_RUNTIME()

CFLAGS(
    -DHAVE_CONFIG_H
    -DEVENT__HAVE_STRLCPY=1
)

END()

RECURSE(
    event_core
    event_extra
    event_openssl
    event_thread
)