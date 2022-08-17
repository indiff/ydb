RESOURCES_LIBRARY()

OWNER(g:contrib heretic)

IF (NOT QT_REQUIRED)
    MESSAGE(FATAL_ERROR "No QT Toolkit for your build")
ELSE()
    IF (OS_LINUX)
        # Qt version 5.15.2 for linux
        DECLARE_EXTERNAL_RESOURCE(QT sbr:2773138831)
    ELSEIF (OS_DARWIN)
        CFLAGS(GLOBAL "-F$QT_RESOURCE_GLOBAL/lib")
        LDFLAGS("-F$QT_RESOURCE_GLOBAL/lib")
        # Qt version 5.15.2 for darwin
        DECLARE_EXTERNAL_RESOURCE(QT sbr:2773128565)
    ELSE()
        ENABLE(QT_NOT_FOUND)
    ENDIF()

    IF (HOST_OS_LINUX)
        DECLARE_EXTERNAL_RESOURCE(HOST_QT sbr:2773138831)
    ELSEIF (HOST_OS_DARWIN)
        DECLARE_EXTERNAL_RESOURCE(HOST_QT sbr:2773128565)
    ELSE()
        ENABLE(QT_NOT_FOUND)
    ENDIF()

    IF (QT_NOT_FOUND)
        MESSAGE(FATAL_ERROR "No QT Toolkit for the selected platform")
    ELSE()
        CFLAGS(GLOBAL "-isystem$QT_RESOURCE_GLOBAL/include")
        LDFLAGS("-L$QT_RESOURCE_GLOBAL/lib")
    ENDIF()
ENDIF()

END()