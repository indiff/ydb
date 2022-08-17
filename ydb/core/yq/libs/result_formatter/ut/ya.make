UNITTEST_FOR(ydb/core/yq/libs/result_formatter)

OWNER(
    g:kikimr
    g:yq
)

FORK_SUBTESTS()

IF (SANITIZER_TYPE OR WITH_VALGRIND)
    SIZE(MEDIUM)
ENDIF()

SRCS(
    result_formatter_ut.cpp
)

PEERDIR(
    ydb/library/yql/public/udf/service/stub
    ydb/services/ydb
)

YQL_LAST_ABI_VERSION()

END()