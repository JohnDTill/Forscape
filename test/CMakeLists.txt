cmake_minimum_required(VERSION 3.5)

project(HopeTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(TEST .)
set(BASE ..)
set(INCLUDE ${BASE}/include)
set(SRC ${BASE}/src)
set(CONSTRUCT ${SRC}/typeset_constructs)
set(COMMAND ${SRC}/typeset_commands)
include_directories(${INCLUDE})
include_directories(${SRC})
include_directories(${CONSTRUCT})
include_directories(${COMMAND})

configure_file(${TEST}/serial_valid.txt ${CMAKE_BINARY_DIR} COPYONLY)
configure_file(${TEST}/serial_malformed.txt ${CMAKE_BINARY_DIR} COPYONLY)
configure_file(${TEST}/serial_exhaustive.txt ${CMAKE_BINARY_DIR} COPYONLY)

add_compile_definitions(TYPESET_MEMORY_DEBUG HOPE_SEMANTIC_DEBUGGING)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)
find_package(Qt5Svg REQUIRED)

add_executable(HopeTests ${TEST}/test_driver.cpp
    ${TEST}/code_interpreter.h
    ${TEST}/code_parser.h
    ${TEST}/code_scanner.h
    ${TEST}/hope_benchmark.h
    ${TEST}/report.h
    ${TEST}/serial.h
    ${TEST}/typeset.h
    ${TEST}/typeset_control.h
    ${TEST}/typeset_graphics.h
    ${TEST}/typeset_loadsave.h
    ${TEST}/typeset_mutability.h
    ${TEST}/typeset_qpainter.h
    ${INCLUDE}/hope_construct_codes.h
    ${SRC}/hope_error.cpp
    ${INCLUDE}/hope_error.h
    ${SRC}/hope_error_types.h
    ${SRC}/hope_interpreter.cpp
    ${SRC}/hope_interpreter.h
    ${SRC}/hope_interpreter_gen.cpp
    ${SRC}/hope_parse_tree.cpp
    ${SRC}/hope_parse_tree.h
    ${SRC}/hope_parsenodetype.h
    ${SRC}/hope_parser.cpp
    ${SRC}/hope_parser.h
    ${SRC}/hope_scanner.cpp
    ${SRC}/hope_scanner.h
    ${INCLUDE}/hope_serial.h
    ${INCLUDE}/hope_serial_unicode.h
    ${SRC}/hope_stack.cpp
    ${SRC}/hope_stack.h
    ${SRC}/hope_symbol_table.cpp
    ${SRC}/hope_symbol_table.h
    ${SRC}/hope_tokentype.h
    ${SRC}/hope_unicode.h
    ${INCLUDE}/typeset_command.h
    ${SRC}/typeset_construct.cpp
    ${SRC}/typeset_construct.h
    ${SRC}/typeset_controller.cpp
    ${INCLUDE}/typeset_controller.h
    ${SRC}/typeset_keywords.cpp
    ${SRC}/typeset_keywords.h
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
    ${INCLUDE}/typeset_semantic_tags.h
    ${SRC}/typeset_shorthand.h
    ${SRC}/typeset_subphrase.cpp
    ${SRC}/typeset_subphrase.h
    ${SRC}/typeset_text.cpp
    ${SRC}/typeset_text.h
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

target_link_libraries(HopeTests PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt5::Svg)

target_compile_options(HopeTests PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W3 /WX>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic -Werror>
)

#Added because Eigen unsupported resulted in compiles with too many sections
target_compile_options(HopeTests PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/bigobj /DHOPE_EIGEN_UNSUPPORTED>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-m64 -Wa,-mbig-obj>
)
