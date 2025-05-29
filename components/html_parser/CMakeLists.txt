idf_component_register()

set(LEXBOR_BUILD_SHARED OFF)
set(LEXBOR_BUILD_TESTS_CPP OFF)

add_subdirectory(html_parser)

add_prebuilt_library(html_parser "${CMAKE_CURRENT_BINARY_DIR}/html_parser/liblexbor_static.a")

target_include_directories(html_parser INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/html_parser/source")

target_link_libraries(${COMPONENT_LIB} INTERFACE html_parser)