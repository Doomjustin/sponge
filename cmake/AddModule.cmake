function(spg_add_module module_name)
    if(ARGC LESS 1)
        message(FATAL_ERROR "spg_add_module requires: module_name [module_dir] [PUBLIC ...] [PRIVATE ...] [INTERFACE ...]")
    endif()

    set(module_dir "${CMAKE_CURRENT_SOURCE_DIR}")
    set(scope_start_index 1)

    if(ARGC GREATER 1)
        if(NOT (ARGV1 STREQUAL "PUBLIC" OR ARGV1 STREQUAL "PRIVATE" OR ARGV1 STREQUAL "INTERFACE"))
            set(module_dir_raw ${ARGV1})
            if(IS_ABSOLUTE "${module_dir_raw}")
                set(module_dir "${module_dir_raw}")
            else()
                set(module_dir "${CMAKE_CURRENT_SOURCE_DIR}/${module_dir_raw}")
            endif()
            set(scope_start_index 2)
        endif()
    endif()

    if(ARGC GREATER ${scope_start_index})
        math(EXPR remaining_count "${ARGC} - ${scope_start_index}")
        list(SUBLIST ARGV ${scope_start_index} ${remaining_count} link_args)

        set(current_scope "")
        foreach(arg IN LISTS link_args)
            if(arg STREQUAL "PUBLIC" OR arg STREQUAL "PRIVATE" OR arg STREQUAL "INTERFACE")
                set(current_scope ${arg})
            else()
                if(current_scope STREQUAL "")
                    message(FATAL_ERROR "Link dependency '${arg}' must follow one of: PUBLIC, PRIVATE, INTERFACE")
                endif()

                if(current_scope STREQUAL "PUBLIC")
                    list(APPEND public_libs ${arg})
                elseif(current_scope STREQUAL "PRIVATE")
                    list(APPEND private_libs ${arg})
                else()
                    list(APPEND interface_libs ${arg})
                endif()
            endif()
        endforeach()
    endif()

    file(GLOB module_cpp CONFIGURE_DEPENDS "${module_dir}/*.cpp")
    file(GLOB module_test_cpp CONFIGURE_DEPENDS "${module_dir}/*.test.cpp")

    list(FILTER module_cpp EXCLUDE REGEX "\\.test\\.cpp$")

    if(NOT module_cpp)
        message(FATAL_ERROR "No module sources found in ${module_dir}. Expected at least one *.cpp (excluding *.test.cpp).")
    endif()

    string(TOLOWER "${PROJECT_NAME}" project_name_lc)

    set(lib_target ${project_name_lc}.${module_name})

    if(TARGET ${lib_target})
        message(FATAL_ERROR "Library target already exists: ${lib_target}")
    endif()

    add_library(${lib_target} STATIC ${module_cpp})

    set(module_alias ${project_name_lc}::${module_name})
    if(TARGET ${module_alias})
        message(FATAL_ERROR "Target alias already exists: ${module_alias}")
    endif()
    add_library(${module_alias} ALIAS ${lib_target})

    target_include_directories(${lib_target}
        PUBLIC
            ${PROJECT_SOURCE_DIR}/include
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )
    target_compile_features(${lib_target} PUBLIC cxx_std_23)

    if(public_libs OR private_libs OR interface_libs)
        target_link_libraries(${lib_target}
            PUBLIC
                ${public_libs}
            PRIVATE
                ${private_libs}
            INTERFACE
                ${interface_libs}
        )
    endif()

    if(BUILD_TESTING AND ${PROJECT_NAME}_BUILD_TESTS AND module_test_cpp)
        set(test_target ${project_name_lc}.${module_name}.test)
        add_executable(${test_target} ${module_test_cpp})
        target_link_libraries(${test_target}
            PRIVATE
                ${lib_target}
                Catch2::Catch2WithMain
        )
        target_compile_features(${test_target} PRIVATE cxx_std_23)
        add_test(NAME ${test_target} COMMAND ${test_target})
    endif()
endfunction()
