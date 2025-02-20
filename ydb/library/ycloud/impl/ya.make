RECURSE_FOR_TESTS(
    ut
)

LIBRARY()

SRCS(
    access_service.h
    access_service.cpp
    folder_service.h
    folder_service.cpp
    folder_service_adapter.cpp
    grpc_service_cache.h
    grpc_service_client.h
    grpc_service_settings.h
    iam_token_service.h
    iam_token_service.cpp
    service_account_service.h
    service_account_service.cpp
    user_account_service.h
    user_account_service.cpp
)

PEERDIR(
    ydb/library/ycloud/api
    library/cpp/actors/core
    library/cpp/digest/crc32c
    library/cpp/grpc/client
    library/cpp/json
    ydb/core/base
    ydb/public/lib/deprecated/client
    ydb/public/lib/deprecated/kicli
)

END()
