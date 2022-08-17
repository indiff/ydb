UNITTEST()

FORK_SUBTESTS()

SIZE(MEDIUM)

OWNER(alexvru)

SRCS(
    main.cpp
    self_heal_actor_ut.cpp
    defs.h
    env.h
    events.h
    node_warden_mock.h
    timer_actor.h
    vdisk_mock.h
)

PEERDIR(
    ydb/core/blobstorage/dsproxy/mock
    ydb/core/blobstorage/pdisk/mock
    ydb/core/mind/bscontroller
    ydb/core/tx/scheme_board
    ydb/library/yql/public/udf/service/stub
)

YQL_LAST_ABI_VERSION()

END()