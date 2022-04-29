function(enable_sanitizers project_name)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set(SANITIZERS "")

        if(ENABLE_SANITIZER_ADDRESS)
            list(APPEND SANITIZERS "address")
        endif()

        if(ENABLE_SANITIZER_MEMORY)
            list(APPEND SANITIZERS "memory")
        endif()

        if(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR)
            list(APPEND SANITIZERS "undefined")
        endif()

        if(ENABLE_SANITIZER_THREAD)
            list(APPEND SANITIZERS "thread")
        endif()

        list(JOIN SANITIZERS "," LIST_OF_SANITIZERS)
    endif()

    if(LIST_OF_SANITIZERS)
        if(NOT "${LIST_OF_SANITIZERS}" STREQUAL "")
            target_compile_options(project_options INTERFACE -O0 -g -fsanitize=${LIST_OF_SANITIZERS})
            target_link_libraries(project_options INTERFACE -fsanitize=${LIST_OF_SANITIZERS})
        endif()
    endif()
endfunction()