cmake_minimum_required(VERSION 3.5)

option(TEST_QT OFF)

project(HopeTests LANGUAGES CXX)

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/conan-dependencies ${CMAKE_MODULE_PATH})
include(${CMAKE_BINARY_DIR}/conan-dependencies/conanbuildinfo.cmake)
conan_basic_setup(TARGETS NO_OUTPUT_DIRS KEEP_RPATHS)
include_directories(${CONAN_INCLUDE_DIRS})

find_package(Eigen3 REQUIRED)
#find_package(gmp REQUIRED)
find_package(phmap REQUIRED)
find_package(readerwriterqueue REQUIRED)
find_package(spdlog REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TEST .)
set(BASE ..)
set(META ${BASE}/meta)
set(INCLUDE ${BASE}/include)
set(SRC ${BASE}/src)
set(CONSTRUCT ${SRC}/typeset_constructs)
set(GEN ${SRC}/generated)
set(COMMAND ${SRC}/typeset_commands)
set(EXAMPLE ${BASE}/example/exampleWidgetQt)
include_directories(${INCLUDE})
include_directories(${SRC})
include_directories(${CONSTRUCT})
include_directories(${GEN})
include_directories(${COMMAND})

configure_file(${TEST}/serial_valid.txt ${CMAKE_BINARY_DIR} COPYONLY)
configure_file(${TEST}/serial_malformed.txt ${CMAKE_BINARY_DIR} COPYONLY)
configure_file(${TEST}/serial_exhaustive.txt ${CMAKE_BINARY_DIR} COPYONLY)

add_compile_definitions(TYPESET_MEMORY_DEBUG HOPE_SEMANTIC_DEBUGGING)
add_compile_definitions(EIGEN_NO_AUTOMATIC_RESIZING)
add_compile_definitions(BASE_TEST_DIR="../test/interpreter_scripts")
add_compile_definitions(alloc_except=noexcept)

set(GEN_FILES
    ${GEN}/code_ast_fields.cpp
    ${GEN}/code_ast_fields.h
    ${GEN}/code_error_types.h
    ${GEN}/code_parsenode_ops.h
    ${GEN}/code_tokentype.h
    ${GEN}/construct_codes.h
    ${GEN}/hope_interpreter_gen.cpp
    ${GEN}/semantic_tags.h
    ${GEN}/typeset_closesymbol.h
    ${GEN}/typeset_keywords.cpp
    ${GEN}/typeset_keywords.h
    ${GEN}/typeset_shorthand.cpp
    ${GEN}/typeset_shorthand.h
    ${GEN}/typeset_themes.cpp
    ${GEN}/typeset_themes.h
    ${GEN}/unicode_subscripts.h
    ${GEN}/unicode_superscripts.h
)

set(CONSTRUCT_FILES
    ${CONSTRUCT}/typeset_accentbar.cpp
    ${CONSTRUCT}/typeset_accentbar.h
    ${CONSTRUCT}/typeset_accentbreve.h
    ${CONSTRUCT}/typeset_accentdddot.h
    ${CONSTRUCT}/typeset_accentddot.h
    ${CONSTRUCT}/typeset_accentdot.h
    ${CONSTRUCT}/typeset_accenthat.h
    ${CONSTRUCT}/typeset_accenttilde.h
    ${CONSTRUCT}/typeset_bigcoprod0.h
    ${CONSTRUCT}/typeset_bigcoprod1.h
    ${CONSTRUCT}/typeset_bigcoprod2.h
    ${CONSTRUCT}/typeset_bigintersection0.h
    ${CONSTRUCT}/typeset_bigintersection1.h
    ${CONSTRUCT}/typeset_bigintersection2.h
    ${CONSTRUCT}/typeset_bigprod0.h
    ${CONSTRUCT}/typeset_bigprod1.h
    ${CONSTRUCT}/typeset_bigprod2.h
    ${CONSTRUCT}/typeset_bigsum0.h
    ${CONSTRUCT}/typeset_bigsum1.h
    ${CONSTRUCT}/typeset_bigsum2.h
    ${CONSTRUCT}/typeset_bigunion0.h
    ${CONSTRUCT}/typeset_bigunion1.h
    ${CONSTRUCT}/typeset_bigunion2.h
    ${CONSTRUCT}/typeset_binomial.h
    ${CONSTRUCT}/typeset_cases.h
    ${CONSTRUCT}/typeset_doubleintegral0.h
    ${CONSTRUCT}/typeset_doubleintegral1.h
    ${CONSTRUCT}/typeset_doubleintegralconv0.h
    ${CONSTRUCT}/typeset_doubleintegralconv1.h
    ${CONSTRUCT}/typeset_dualscript.h
    ${CONSTRUCT}/typeset_fraction.h
    ${CONSTRUCT}/typeset_inf.h
    ${CONSTRUCT}/typeset_integral0.h
    ${CONSTRUCT}/typeset_integral1.h
    ${CONSTRUCT}/typeset_integral2.h
    ${CONSTRUCT}/typeset_integralconv0.h
    ${CONSTRUCT}/typeset_integralconv1.h
    ${CONSTRUCT}/typeset_integralconv2.h
    ${CONSTRUCT}/typeset_limit.h
    ${CONSTRUCT}/typeset_markerlink.h
    ${CONSTRUCT}/typeset_matrix.h
    ${CONSTRUCT}/typeset_max.h
    ${CONSTRUCT}/typeset_min.h
    ${CONSTRUCT}/typeset_nrt.h
    ${CONSTRUCT}/typeset_sqrt.h
    ${CONSTRUCT}/typeset_subscript.h
    ${CONSTRUCT}/typeset_sup.h
    ${CONSTRUCT}/typeset_superscript.h
    ${CONSTRUCT}/typeset_tripleintegral0.h
    ${CONSTRUCT}/typeset_tripleintegral1.h
    ${CONSTRUCT}/typeset_tripleintegralconv0.h
    ${CONSTRUCT}/typeset_tripleintegralconv1.h
)

set(SOURCES
    ${TEST}/test_driver.cpp
    ${TEST}/code_interpreter.h
    ${TEST}/code_interpreter_illformed.h
    ${TEST}/code_parser.h
    ${TEST}/code_scanner.h
    ${TEST}/hope_benchmark.h
    ${TEST}/report.h
    ${TEST}/serial.h
    ${TEST}/typeset.h
    ${TEST}/typeset_control.h
    ${TEST}/typeset_loadsave.h
    ${TEST}/typeset_mutability.h
    ${GEN_FILES}
    ${CONSTRUCT_FILES}
    ${SRC}/hope_common.h
    ${SRC}/hope_error.cpp
    ${INCLUDE}/hope_error.h
    ${SRC}/hope_interpreter.cpp
    ${SRC}/hope_interpreter.h
    ${INCLUDE}/hope_logging.h
    ${SRC}/hope_message.h
    ${SRC}/hope_parse_tree.cpp
    ${SRC}/hope_parse_tree.h
    ${SRC}/hope_parser.cpp
    ${SRC}/hope_parser.h
    ${SRC}/hope_scanner.cpp
    ${SRC}/hope_scanner.h
    ${INCLUDE}/hope_serial.h
    ${INCLUDE}/hope_serial_unicode.h
    ${SRC}/hope_stack.cpp
    ${SRC}/hope_stack.h
    ${SRC}/hope_static_pass.cpp
    ${SRC}/hope_static_pass.h
    ${SRC}/hope_symbol_build_pass.cpp
    ${SRC}/hope_symbol_build_pass.h
    ${SRC}/hope_symbol_link_pass.cpp
    ${SRC}/hope_symbol_link_pass.h
    ${SRC}/hope_symbol_table.cpp
    ${SRC}/hope_symbol_table.h
    ${SRC}/hope_token.h
    ${SRC}/hope_unicode.h
    ${SRC}/hope_value.cpp
    ${SRC}/hope_value.h
    ${INCLUDE}/typeset_command.h
    ${SRC}/typeset_construct.cpp
    ${SRC}/typeset_construct.h
    ${SRC}/typeset_controller.cpp
    ${INCLUDE}/typeset_controller.h
    ${SRC}/typeset_line.cpp
    ${SRC}/typeset_line.h
    ${SRC}/typeset_marker.cpp
    ${SRC}/typeset_marker.h
    ${SRC}/typeset_model.cpp
    ${INCLUDE}/typeset_model.h
    ${SRC}/typeset_phrase.cpp
    ${SRC}/typeset_phrase.h
    ${SRC}/typeset_selection.cpp
    ${SRC}/typeset_selection.h
    ${SRC}/typeset_subphrase.cpp
    ${SRC}/typeset_subphrase.h
    ${SRC}/typeset_text.cpp
    ${SRC}/typeset_text.h
    ${COMMAND}/typeset_command_indent.cpp
    ${COMMAND}/typeset_command_indent.h
    ${COMMAND}/typeset_command_line.cpp
    ${COMMAND}/typeset_command_line.h
    ${COMMAND}/typeset_command_list.cpp
    ${COMMAND}/typeset_command_list.h
    ${COMMAND}/typeset_command_pair.cpp
    ${COMMAND}/typeset_command_pair.h
    ${COMMAND}/typeset_command_phrase.cpp
    ${COMMAND}/typeset_command_phrase.h
    ${COMMAND}/typeset_command_text.cpp
    ${COMMAND}/typeset_command_text.h
    ${COMMAND}/typeset_insert_chars.cpp
    ${COMMAND}/typeset_insert_chars.h
    ${COMMAND}/typeset_remove_chars.cpp
    ${COMMAND}/typeset_remove_chars.h
)

if (${TEST_QT})
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)
    find_package(Qt${QT_VERSION_MAJOR}Svg REQUIRED)
    add_compile_definitions(TEST_QT)
    qt_add_resources(EMBEDDED_RESOURCES ${EXAMPLE}/resources.qrc)
    list(APPEND SOURCES
        ${EMBEDDED_RESOURCES}
        ${SRC}/typeset_painter.h
        ${SRC}/typeset_painter_qt.cpp
        ${SRC}/typeset_view.cpp
        ${INCLUDE}/typeset_view.h
        ${EXAMPLE}/symboltreeview.cpp
        ${EXAMPLE}/symboltreeview.h
    )
else()
    add_compile_definitions(HOPE_TYPESET_HEADLESS)
endif ()

add_executable(HopeTests ${SOURCES})

FIND_PACKAGE ( Threads REQUIRED )

if (${TEST_QT})
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${CONAN_LIBS}
        Qt${QT_VERSION_MAJOR}::Svg
        Qt${QT_VERSION_MAJOR}::Widgets
        Threads::Threads
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${CONAN_LIBS}
        Threads::Threads
    )
endif ()

add_test(NAME IntegrationTest COMMAND HopeTests.exe)

add_compile_definitions($<$<CONFIG:Release>:EIGEN_NO_DEBUG>)

if(MSVC)
    target_compile_options(HopeTests PRIVATE
        /Ob3 /W3 #/WX #EVENTUALLY: address warnings
        $<$<CONFIG:Release>:/Oi /Oy> #/GL
        $<$<CONFIG:Debug>:/bigobj> #Often needed to make Eigen unsupported compile
    )
    target_link_options(HopeTests PRIVATE
        $<$<CONFIG:Release>:/OPT:REF> #/LTCG
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if (WIN32)
        #Need optimisation for MinGW to compile w/ Eigen unsupported
        target_compile_options(HopeTests PRIVATE $<$<CONFIG:Debug>:-O1>)
    endif()
    target_compile_options(HopeTests PRIVATE
        -Wall -Wextra -pedantic #-Werror #EVENTUALLY: address warnings
        $<$<CONFIG:Release>: -Ofast -fmodulo-sched -fmodulo-sched-allow-regmoves -fgcse-sm -fgcse-las -fdelete-null-pointer-checks
        -fomit-frame-pointer -funroll-loops> #-fwhole-program #EVENTUALLY: couldn't get whole program optimisation working
    )
    target_link_options(HopeTests PRIVATE
        -Wa,-mbig-obj
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # using Clang
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    # using Intel C++
endif()

add_custom_target(
    codegen ALL
    COMMAND python3 all.py
    WORKING_DIRECTORY ${META}
    BYPRODUCTS ${GEN_FILES}
    COMMENT "Performing codegen"
)

add_dependencies(HopeTests codegen)
