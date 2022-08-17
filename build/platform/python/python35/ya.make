RESOURCES_LIBRARY()

OWNER(
    g:contrib
    g:yatool
)

INCLUDE(${ARCADIA_ROOT}/build/platform/python/resources.inc)

IF (OS_LINUX)
    DECLARE_EXTERNAL_RESOURCE(EXTERNAL_PYTHON35 ${PYTHON35_LINUX})
ELSEIF (OS_DARWIN)
    DECLARE_EXTERNAL_RESOURCE(EXTERNAL_PYTHON35 ${PYTHON35_DARWIN})
ELSEIF (OS_WINDOWS)
    DECLARE_EXTERNAL_RESOURCE(EXTERNAL_PYTHON35 ${PYTHON35_WINDOWS})
ENDIF()

END()