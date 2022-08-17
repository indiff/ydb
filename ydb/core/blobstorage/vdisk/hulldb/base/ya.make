LIBRARY()

OWNER(fomichev)

PEERDIR(
    library/cpp/monlib/service/pages
    ydb/core/base
    ydb/core/blobstorage/pdisk
)

SRCS(
    blobstorage_blob.h
    blobstorage_hulldefs.cpp
    blobstorage_hulldefs.h
    blobstorage_hullsatisfactionrank.cpp
    blobstorage_hullsatisfactionrank.h
    blobstorage_hullstorageratio.h
    defs.h
    hullbase_barrier.cpp
    hullbase_barrier.h
    hullbase_block.h
    hullbase_logoblob.h
    hullds_arena.h
    hullds_generic_it.h
    hullds_glue.h
    hullds_settings.h
)

END()

RECURSE_FOR_TESTS(
    ut
)