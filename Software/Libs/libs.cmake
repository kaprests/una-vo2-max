file(GLOB_RECURSE LIBS_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_LIST_DIR}/Sources/*.c
    ${CMAKE_CURRENT_LIST_DIR}/Sources/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/*.c
    ${CMAKE_CURRENT_LIST_DIR}/Source/*.cpp
)

# common/: the contract shared by the service and GUI processes.
# service/: service-only headers; the firmware GUI build does not see them.
set(LIBS_COMMON_INCLUDE_DIRS  ${CMAKE_CURRENT_LIST_DIR}/Header/common)
set(LIBS_SERVICE_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/Header/service)
set(LIBS_INCLUDE_DIRS ${LIBS_COMMON_INCLUDE_DIRS} ${LIBS_SERVICE_INCLUDE_DIRS})