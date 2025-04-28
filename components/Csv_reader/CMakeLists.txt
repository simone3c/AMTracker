idf_component_register()

add_compile_definitions(INTERNAL_GETLINE)

add_subdirectory(Csv_reader)

add_prebuilt_library(csv "${CMAKE_CURRENT_BINARY_DIR}/Csv_reader/src/libcsv_reader.a")

target_include_directories(csv INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Csv_reader/src/include")

target_link_libraries(${COMPONENT_LIB} INTERFACE csv)