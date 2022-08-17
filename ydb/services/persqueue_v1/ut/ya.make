UNITTEST_FOR(ydb/services/persqueue_v1)

OWNER(
    alexnick
    g:kikimr
    g:logbroker
)

CFLAGS(
    -DACTORLIB_HUGE_PB_SIZE
)

FORK_SUBTESTS()

IF (SANITIZER_TYPE OR WITH_VALGRIND)
    TIMEOUT(1800)
    SIZE(LARGE)
    TAG(ya:fat)
    REQUIREMENTS(ram:32)
ELSE()
    TIMEOUT(600)
    SIZE(MEDIUM)
ENDIF()

SRCS(
    persqueue_ut.cpp
    persqueue_common_ut.cpp
    test_utils.h
    pq_data_writer.h
    api_test_setup.h
    rate_limiter_test_setup.h
    rate_limiter_test_setup.cpp
)

PEERDIR(
    library/cpp/getopt
    library/cpp/svnversion
    ydb/core/testlib
    ydb/library/aclib
    ydb/library/persqueue/topic_parser
    ydb/public/api/grpc
    ydb/public/sdk/cpp/client/ydb_persqueue_core/ut/ut_utils
    ydb/public/sdk/cpp/client/ydb_persqueue_public
    ydb/public/sdk/cpp/client/ydb_table
    ydb/services/persqueue_v1
)

YQL_LAST_ABI_VERSION()

END()