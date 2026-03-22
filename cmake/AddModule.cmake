function(spg_add_module module_name)
    if(NOT (ARGC EQUAL 1 OR ARGC EQUAL 2))
        message(FATAL_ERROR "spg_add_module requires: module_name [module_dir]")
    endif()

    if(ARGC EQUAL 2)
        set(module_dir_raw ${ARGV1})
        if(IS_ABSOLUTE "${module_dir_raw}")
            set(module_dir "${module_dir_raw}")
        else()
            set(module_dir "${CMAKE_CURRENT_SOURCE_DIR}/${module_dir_raw}")
        endif()
    else()
        set(module_dir "${CMAKE_CURRENT_SOURCE_DIR}")
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
