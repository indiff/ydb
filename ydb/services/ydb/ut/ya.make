UNITTEST_FOR(ydb/services/ydb)

OWNER(
    dcherednik
    g:kikimr
)

FORK_SUBTESTS()

IF (SANITIZER_TYPE OR WITH_VALGRIND)
    SPLIT_FACTOR(60)
    TIMEOUT(3600)
    SIZE(LARGE)
    TAG(ya:fat)
ELSE()
    TIMEOUT(300)
    SIZE(MEDIUM)
ENDIF()

SRCS(
    ydb_bulk_upsert_ut.cpp
    ydb_bulk_upsert_olap_ut.cpp
    ydb_coordination_ut.cpp
    ydb_index_table_ut.cpp
    ydb_import_ut.cpp
    ydb_ut.cpp
    ydb_s3_internal_ut.cpp
    ydb_scripting_ut.cpp
    ydb_table_ut.cpp
    ydb_table_split_ut.cpp
    ydb_stats_ut.cpp
    ydb_long_tx_ut.cpp
    ydb_logstore_ut.cpp
    ydb_olapstore_ut.cpp
    json_udf.cpp
    re2_udf.cpp
)

PEERDIR(
    contrib/libs/apache/arrow
    library/cpp/getopt
    library/cpp/grpc/client
    library/cpp/regex/pcre
    library/cpp/svnversion
    ydb/core/kqp/ut/common
    ydb/core/testlib
    ydb/library/yql/minikql/dom
    ydb/library/yql/minikql/jsonpath
    ydb/public/lib/experimental
    ydb/public/lib/yson_value
    ydb/public/sdk/cpp/client/draft
    ydb/public/sdk/cpp/client/ydb_coordination
    ydb/public/sdk/cpp/client/ydb_export
    ydb/public/sdk/cpp/client/ydb_extension
    ydb/public/sdk/cpp/client/ydb_operation
    ydb/public/sdk/cpp/client/ydb_scheme
    ydb/services/ydb
)

YQL_LAST_ABI_VERSION()

REQUIREMENTS(ram:14)

END()