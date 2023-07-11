cmake_minimum_required(VERSION 3.5)

project(Forscape VERSION 0.0.3 LANGUAGES CXX)
set(EXE_NAME "Forscape")

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/conan-dependencies ${CMAKE_MODULE_PATH})
include(${CMAKE_BINARY_DIR}/conan-dependencies/conanbuildinfo.cmake)
conan_basic_setup(NO_OUTPUT_DIRS KEEP_RPATHS)
include_directories(${CONAN_INCLUDE_DIRS})

find_package(Eigen3 REQUIRED)
find_package(phmap REQUIRED)
find_package(readerwriterqueue REQUIRED)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#add_compile_definitions(FORSCAPE_TYPESET_LAYOUT_DEBUG)
#add_compile_definitions(TYPESET_MEMORY_DEBUG)
#add_compile_definitions(FORSCAPE_LOAD_WITHOUT_CACHE_DEBUG)

add_compile_definitions(alloc_except=noexcept)
add_compile_definitions(EIGEN_NO_AUTOMATIC_RESIZING)

set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/forscape.rc")

set(BASE ..)
set(META ${BASE}/meta)
set(SRC ${BASE}/src)
set(CONSTRUCT ${SRC}/typeset_constructs)
set(GEN ${SRC}/generated)
set(COMMAND ${SRC}/typeset_commands)
include_directories(${SRC})
include_directories(${CONSTRUCT})
include_directories(${GEN})
include_directories(${COMMAND})

find_package(QT NAMES Qt5 Qt6 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR}Svg REQUIRED)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    find_package(QT NAMES Qt6 COMPONENTS SvgWidgets REQUIRED)
    find_package(Qt${QT_VERSION_MAJOR} COMPONENTS SvgWidgets REQUIRED)
    qt6_add_resources(EMBEDDED_RESOURCES resources.qrc)
else()
    add_compile_definitions(QT5)
    qt5_add_resources(EMBEDDED_RESOURCES resources.qrc)
endif()

set(GEN_FILES
    ${GEN}/code_ast_fields.cpp
    ${GEN}/code_ast_fields.h
    ${GEN}/code_error_types.h
    ${GEN}/code_parsenode_ops.h
    ${GEN}/code_tokentype.h
    ${GEN}/construct_codes.h
    ${GEN}/forscape_interpreter_gen.cpp
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
    ${CONSTRUCT}/typeset_big_integral.h
    ${CONSTRUCT}/typeset_big_symbol.h
    ${CONSTRUCT}/typeset_binomial.h
    ${CONSTRUCT}/typeset_cases.h
    ${CONSTRUCT}/typeset_dualscript.h
    ${CONSTRUCT}/typeset_fraction.h
    ${CONSTRUCT}/typeset_inf.h
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
)

set(PROJECT_SOURCES
    main.cpp
    keywordsubstitutioneditor.cpp
    keywordsubstitutioneditor.h
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    mathtoolbar.h
    mathtoolbar.cpp
    plot.cpp
    plot.h
    preferences.cpp
    preferences.h
    preferences.ui
    qgraphvizcall.h
    searchdialog.cpp
    searchdialog.h
    searchdialog.ui
    splitter.cpp
    splitter.h
    symbolsubstitutioneditor.cpp
    symbolsubstitutioneditor.h
    symboltreeview.cpp
    symboltreeview.h
    ${EMBEDDED_RESOURCES}
    ${GEN_FILES}
    ${CONSTRUCT_FILES}
    ${SRC}/forscape_common.h
    ${SRC}/forscape_dynamic_settings.cpp
    ${SRC}/forscape_dynamic_settings.h
    ${SRC}/forscape_error.cpp
    ${SRC}/forscape_error.h
    ${SRC}/forscape_interpreter.cpp
    ${SRC}/forscape_interpreter.h
    ${SRC}/forscape_message.h
    ${SRC}/forscape_parse_tree.cpp
    ${SRC}/forscape_parse_tree.h
    ${SRC}/forscape_parser.cpp
    ${SRC}/forscape_parser.h
    ${SRC}/forscape_program.cpp
    ${SRC}/forscape_program.h
    ${SRC}/forscape_scanner.cpp
    ${SRC}/forscape_scanner.h
    ${SRC}/forscape_serial.h
    ${SRC}/forscape_serial_unicode.h
    ${SRC}/forscape_stack.cpp
    ${SRC}/forscape_stack.h
    ${SRC}/forscape_static_pass.cpp
    ${SRC}/forscape_static_pass.h
    ${SRC}/forscape_symbol_lexical_pass.cpp
    ${SRC}/forscape_symbol_lexical_pass.h
    ${SRC}/forscape_symbol_link_pass.cpp
    ${SRC}/forscape_symbol_link_pass.h
    ${SRC}/forscape_symbol_table.cpp
    ${SRC}/forscape_symbol_table.h
    ${SRC}/forscape_token.h
    ${SRC}/forscape_unicode.h
    ${SRC}/forscape_value.cpp
    ${SRC}/forscape_value.h
    ${SRC}/typeset_command.h
    ${SRC}/typeset_construct.cpp
    ${SRC}/typeset_construct.h
    ${SRC}/typeset_controller.cpp
    ${SRC}/typeset_controller.h
    ${SRC}/typeset_integral_preference.h
    ${SRC}/typeset_line.cpp
    ${SRC}/typeset_line.h
    ${SRC}/typeset_marker.cpp
    ${SRC}/typeset_marker.h
    ${SRC}/typeset_model.cpp
    ${SRC}/typeset_model.h
    ${SRC}/typeset_painter.h
    ${SRC}/typeset_painter_qt.cpp
    ${SRC}/typeset_parser.cpp
    ${SRC}/typeset_parser.h
    ${SRC}/typeset_phrase.cpp
    ${SRC}/typeset_phrase.h
    ${SRC}/typeset_selection.cpp
    ${SRC}/typeset_selection.h
    ${SRC}/typeset_subphrase.cpp
    ${SRC}/typeset_subphrase.h
    ${SRC}/typeset_syntax.h
    ${SRC}/typeset_text.cpp
    ${SRC}/typeset_text.h
    ${SRC}/typeset_view.cpp
    ${SRC}/typeset_view.h
    ${SRC}/qt_compatability.h
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
    ${COMMAND}/typeset_command_replace_construct.h
    ${COMMAND}/typeset_command_text.cpp
    ${COMMAND}/typeset_command_text.h
    ${COMMAND}/typeset_insert_chars.cpp
    ${COMMAND}/typeset_insert_chars.h
    ${COMMAND}/typeset_remove_chars.cpp
    ${COMMAND}/typeset_remove_chars.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Forscape
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(Forscape SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(Forscape
            ${PROJECT_SOURCES}
            ${APP_ICON_RESOURCE_WINDOWS}
        )
    endif()
endif()

FIND_PACKAGE ( Threads REQUIRED )
target_link_libraries(Forscape PRIVATE
    ${CONAN_LIBS}
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Svg
    Threads::Threads
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    target_link_libraries(Forscape PRIVATE
        Qt${QT_VERSION_MAJOR}::SvgWidgets
    )
endif()

set_target_properties(Forscape PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set_property(TARGET Forscape PROPERTY WIN32_EXECUTABLE true)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Forscape)
endif()

add_compile_definitions($<$<CONFIG:Release>:EIGEN_NO_DEBUG>)

if(MSVC)
    target_compile_options(Forscape PRIVATE
        /Ob3 /W3 #/WX #EVENTUALLY: address warnings
        $<$<CONFIG:Release>:/Oi /Oy /GL>
        $<$<CONFIG:Debug>:/bigobj> #Often needed to make Eigen unsupported compile
    )
    target_link_options(Forscape PRIVATE
        $<$<CONFIG:Release>:/OPT:REF /LTCG>
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Forscape PRIVATE
        $<$<CONFIG:Debug>:-O1> #Need optimisation for MinGW to compile w/ Eigen unsupported
        -Wall -Wextra -pedantic #-Werror #EVENTUALLY: address warnings
        $<$<CONFIG:Release>: -Ofast -fmodulo-sched -fmodulo-sched-allow-regmoves -fgcse-sm -fgcse-las -fdelete-null-pointer-checks
        -fomit-frame-pointer -funroll-loops -fvisibility=hidden -fwhole-program -flto>
    )
    target_link_options(Forscape PRIVATE
        -Wa,-mbig-obj
    )
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # using Clang
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    # using Intel C++
endif()

set_target_properties(Forscape PROPERTIES OUTPUT_NAME ${EXE_NAME})

add_custom_target(
    codegen ALL
    COMMAND python3 all.py
    WORKING_DIRECTORY ${META}
    BYPRODUCTS ${GEN_FILES}
    COMMENT "Performing codegen"
)

add_dependencies(Forscape codegen)

if (${MAKE_DEB})
    install(TARGETS Forscape RUNTIME)
    include(Packing.cmake)
endif ()
