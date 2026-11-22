cmake_minimum_required(VERSION 3.18)
project(facet-lockscreen VERSION 0.1.0 LANGUAGES CXX)
set(LS_VERSION_SUFFIX "alpha")  # pre-release tag; empty for final releases; keep manifest.json in sync

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(LS_STATIC "Link fully static (portable release binaries)" OFF)

if(LS_VERSION_SUFFIX)
    set(LS_VERSION "${PROJECT_VERSION}-${LS_VERSION_SUFFIX}")
else()
    set(LS_VERSION "${PROJECT_VERSION}")
endif()

# The plugin SDK lives in facet-core: a local checkout next to this project
# (or FACET_CORE_DIR) is used when present, otherwise it is downloaded.
set(FACET_CORE_DIR "" CACHE PATH "facet-core checkout providing the SDK (default: ../facet-core, else download)")
set(FACET_CORE_REF "v0.9.0-alpha" CACHE STRING "facet-core git ref to download the SDK from (lock screens need 0.9+)")
if(NOT FACET_CORE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../facet-core/sdk/CMakeLists.txt")
    set(FACET_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../facet-core")
endif()
if(FACET_CORE_DIR)
    add_subdirectory(${FACET_CORE_DIR}/sdk ${CMAKE_BINARY_DIR}/facet_sdk)
else()
    include(FetchContent)
    FetchContent_Declare(facet_core
        GIT_REPOSITORY https://github.com/siakinnik/facet-core.git
        GIT_TAG ${FACET_CORE_REF}
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR sdk)
    FetchContent_MakeAvailable(facet_core)
endif()

add_library(ls_secret STATIC src/secret.cpp)
target_include_directories(ls_secret PUBLIC src)
target_link_libraries(ls_secret PUBLIC facet_sdk)
target_compile_options(ls_secret PRIVATE -Wall -Wextra)

add_executable(lockscreen src/main.cpp src/i18n/ru.cpp)
target_link_libraries(lockscreen PRIVATE ls_secret)
target_compile_options(lockscreen PRIVATE -Wall -Wextra)
target_compile_definitions(lockscreen PRIVATE LS_VERSION="${LS_VERSION}")
if(LS_STATIC)
    target_link_options(lockscreen PRIVATE -static -s)
endif()

# The build dir doubles as a plugin dir: FACET_PLUGIN_PATH=<build>
set(PLUGIN_OUT ${CMAKE_BINARY_DIR}/lockscreen.plugin)
set_target_properties(lockscreen PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PLUGIN_OUT})
facet_plugin_manifest(${CMAKE_CURRENT_SOURCE_DIR}/manifest.json ${PLUGIN_OUT}/manifest.json)

enable_testing()
add_executable(secret_test tests/secret_test.cpp)
target_link_libraries(secret_test PRIVATE ls_secret)
add_test(NAME secret COMMAND secret_test)

install(TARGETS lockscreen RUNTIME DESTINATION lib/facet/plugins/lockscreen)
install(FILES ${PLUGIN_OUT}/manifest.json DESTINATION lib/facet/plugins/lockscreen)
