UNITTEST_FOR(library/cpp/blockcodecs)

OWNER(pg)

FORK_TESTS()

FORK_SUBTESTS()

SPLIT_FACTOR(40)

TIMEOUT(300)

SIZE(MEDIUM)

SRCS(
    codecs_ut.cpp
)

END()