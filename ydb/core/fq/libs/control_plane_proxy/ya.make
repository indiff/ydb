LIBRARY()

SRCS(
    config.cpp
    control_plane_proxy.cpp
    probes.cpp
)

PEERDIR(
    library/cpp/actors/core
    ydb/core/base
    ydb/core/fq/libs/actors/logging
    ydb/core/fq/libs/actors
    ydb/core/fq/libs/control_plane_config
    ydb/core/fq/libs/control_plane_proxy/events
    ydb/core/fq/libs/control_plane_storage
    ydb/core/fq/libs/rate_limiter/events
    ydb/core/mon
    ydb/library/folder_service
    ydb/library/security
)

YQL_LAST_ABI_VERSION()

END()

RECURSE(
    events
)

RECURSE_FOR_TESTS(
    ut
)
