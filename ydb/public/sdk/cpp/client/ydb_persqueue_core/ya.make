LIBRARY()

OWNER(
    g:kikimr
    g:logbroker
)

GENERATE_ENUM_SERIALIZATION(ydb/public/sdk/cpp/client/ydb_persqueue_core/persqueue.h)

SRCS(
    persqueue.h
    proto_accessor.cpp
)

PEERDIR(
    library/cpp/retry
    ydb/public/sdk/cpp/client/ydb_persqueue_core/impl
    ydb/public/sdk/cpp/client/ydb_proto
    ydb/public/sdk/cpp/client/ydb_driver
)

END()

RECURSE_FOR_TESTS(
    ut
    ut/with_offset_ranges_mode_ut
)