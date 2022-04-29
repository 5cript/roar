function(set_test_target_outputs TARGET)
    set_target_properties(${TARGET} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/lib"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/bin"
    )
endfunction()

add_library(test-settings INTERFACE)

target_compile_definitions(
    test-settings
    INTERFACE 
    TEST_TEMPORARY_DIRECTORY="${CMAKE_BINARY_DIR}/tmp"
)