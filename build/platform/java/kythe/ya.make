RESOURCES_LIBRARY()
OWNER(g:ymake)

IF(USE_SYSTEM_KYTHE)
    MESSAGE(WARNING System Kythe $USE_SYSTEM_KYTHE will be used)
ELSE()
    DECLARE_EXTERNAL_RESOURCE(KYTHE sbr:837801347)
ENDIF()

END()