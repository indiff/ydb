RESOURCES_LIBRARY()

OWNER(g:mapkit)

IF (OS_ANDROID)
    DECLARE_EXTERNAL_RESOURCE(MAPSMOBI_MAVEN_REPO sbr:2586526945)
ELSE()
    MESSAGE(FATAL_ERROR Unsupported platform)
ENDIF()

END()