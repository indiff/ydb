UNITTEST_FOR(ydb/library/yql/udfs/common/stat/static)

OWNER(xenoxeno)

SRCS(
    ../stat_udf_ut.cpp
)

PEERDIR(
    ydb/library/yql/minikql
    ydb/library/yql/minikql/comp_nodes
    ydb/library/yql/minikql/computation
    ydb/library/yql/public/udf/service/exception_policy
)

YQL_LAST_ABI_VERSION()

TIMEOUT(300)

SIZE(MEDIUM)

END()