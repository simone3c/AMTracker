idf_component_register(SRCS "Csv_reader/src/csv_reader.c" INCLUDE_DIRS Csv_reader/src/include)

add_compile_definitions(INTERNAL_GETLINE)

add_subdirectory(Csv_reader)