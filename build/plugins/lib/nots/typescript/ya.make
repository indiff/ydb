PY23_LIBRARY()

OWNER(dankolesnikov)

PY_SRCS(
    __init__.py
    tsc_wrapper.py
)

PEERDIR(
    build/plugins/lib/nots/package_manager
)

END()

RECURSE_FOR_TESTS(
    tests
)