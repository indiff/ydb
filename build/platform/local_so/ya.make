RESOURCES_LIBRARY()

OWNER(g:ymake)

IF (NOT OS_WINDOWS)
    SET_APPEND(RPATH_GLOBAL '-Wl,-rpath,$ORIGIN')
ENDIF()

END()