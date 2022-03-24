cmake_minimum_required(VERSION 3.5)

project(Forscape VERSION 0.0.1 LANGUAGES CXX)
set(EXE_NAME "Forscape")

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#add_compile_definitions(HOPE_TYPESET_LAYOUT_DEBUG)

set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/forscape.rc")

set(BASE ../..)
set(META ${BASE}/meta)
set(INCLUDE ${BASE}/include)
set(SRC ${BASE}/src)
set(CONSTRUCT ${SRC}/typeset_constructs)
set(GEN ${SRC}/generated)
set(COMMAND ${SRC}/typeset_commands)
include_directories(${INCLUDE})
include_directories(${SRC})
include_directories(${CONSTRUCT})
include_directories(${GEN})
include_directories(${COMMAND})

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
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
    ${GEN}/hope_interpreter_gen.cpp
    ${GEN}/semantic_tags.h
    ${GEN}/typeset_closesymbol.h
    ${GEN}/typeset_keywords.cpp
    ${GEN}/typeset_keywords.h
    ${GEN}/typeset_shorthand.h
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

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    mathtoolbar.h
    mathtoolbar.cpp
    preferences.cpp
    preferences.h
    preferences.ui
    qgraphvizcall.h
    searchdialog.cpp
    searchdialog.h
    searchdialog.ui
    symboltreeview.cpp
    symboltreeview.h
    ${EMBEDDED_RESOURCES}
    ${GEN_FILES}
    ${CONSTRUCT_FILES}
    ${SRC}/hope_error.cpp
    ${INCLUDE}/hope_error.h
    ${SRC}/hope_interpreter.cpp
    ${SRC}/hope_interpreter.h
    ${INCLUDE}/hope_logging.h
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
    ${SRC}/hope_symbol_build_pass.cpp
    ${SRC}/hope_symbol_build_pass.h
    ${SRC}/hope_symbol_link_pass.cpp
    ${SRC}/hope_symbol_link_pass.h
    ${SRC}/hope_symbol_table.cpp
    ${SRC}/hope_symbol_table.h
    ${SRC}/hope_type_resolver.cpp
    ${SRC}/hope_type_resolver.h
    ${SRC}/hope_unicode.h
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
    ${SRC}/typeset_painter.h
    ${SRC}/typeset_painter_qt.cpp
    ${SRC}/typeset_phrase.cpp
    ${SRC}/typeset_phrase.h
    ${SRC}/typeset_selection.cpp
    ${SRC}/typeset_selection.h
    ${SRC}/typeset_subphrase.cpp
    ${SRC}/typeset_subphrase.h
    ${SRC}/typeset_text.cpp
    ${SRC}/typeset_text.h
    ${SRC}/typeset_themes.cpp
    ${SRC}/typeset_themes.h
    ${SRC}/typeset_view.cpp
    ${INCLUDE}/typeset_view.h
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

target_link_libraries(Forscape PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Svg
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    target_link_libraries(Forscape PRIVATE
        Qt${QT_VERSION_MAJOR}::SvgWidgets
    )
endif()

FIND_PACKAGE ( Threads REQUIRED )
target_link_libraries(Forscape PRIVATE Threads::Threads)

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

#target_compile_options(Forscape PRIVATE
#  $<$<CXX_COMPILER_ID:MSVC>:/W3 /WX>
#  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic -Werror>
#)

#Added because Eigen unsupported resulted in compiles with too many sections
target_compile_options(Forscape PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
)

add_compile_definitions(HOPE_EIGEN_UNSUPPORTED)

#Errors should be rendered impossible in Eigen
add_compile_options(
  "$<$<CONFIG:RELEASE>:-DEIGEN_NO_DEBUG>"
)

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
