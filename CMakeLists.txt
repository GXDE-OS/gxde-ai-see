cmake_minimum_required(VERSION 3.20)

project(gxde-ai-see
    VERSION 0.1.0
    DESCRIPTION "GXDE AI See - 小埃同学"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network DBus Svg)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DTK2WIDGET REQUIRED dtk2widget)
pkg_check_modules(ICU REQUIRED icu-i18n icu-uc)
find_package(LayerShellQt QUIET)

set(GXDE_AI_SEE_SOURCES
    src/main.cpp
    src/app.cpp
    src/config.cpp
    src/staging/stagingmodel.cpp
    src/portal/portalclient.cpp
    src/factory/linktools.cpp
    src/factory/contentclassifier.cpp
    src/factory/factoryactions.cpp
    src/factory/contentfactory.cpp
    src/dialogs/flowlayout.cpp
    src/dialogs/segmentdialog.cpp
    src/dialogs/speechdialog.cpp
    src/actions/downloader.cpp
    src/actions/texttools.cpp
    src/todo/todomodel.cpp
    src/todo/todowindow.cpp
    src/chat/chatclient.cpp
    src/chat/chatwindow.cpp
    src/chat/mcpclient.cpp
    src/chat/markdown.cpp
    src/chat/chatview.cpp
    src/ui/portalpanel.cpp
    src/ui/aboutdialog.cpp
)

add_executable(gxde-ai-see ${GXDE_AI_SEE_SOURCES})

target_include_directories(gxde-ai-see PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${DTK2WIDGET_INCLUDE_DIRS}
    ${ICU_INCLUDE_DIRS}
)

target_link_libraries(gxde-ai-see PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::DBus
    Qt6::Svg
    ${DTK2WIDGET_LIBRARIES}
    ${ICU_LIBRARIES}
)

target_link_directories(gxde-ai-see PRIVATE ${DTK2WIDGET_LIBRARY_DIRS})

if (LayerShellQt_FOUND)
    target_link_libraries(gxde-ai-see PRIVATE LayerShellQt::Interface)
    target_compile_definitions(gxde-ai-see PRIVATE GXDE_HAVE_LAYERSHELLQT=1)
endif ()

add_executable(gxde-ai-see-selftest
    tests/selftest.cpp
    src/factory/linktools.cpp
    src/factory/contentclassifier.cpp
    src/factory/factoryactions.cpp
    src/actions/texttools.cpp
)
target_include_directories(gxde-ai-see-selftest PRIVATE src ${ICU_INCLUDE_DIRS})
target_link_libraries(gxde-ai-see-selftest PRIVATE Qt6::Core Qt6::Gui ${ICU_LIBRARIES})

add_executable(gxde-ai-see-chattest
    tests/chattest.cpp
    src/chat/chatclient.cpp
    src/chat/mcpclient.cpp
    src/config.cpp
)
target_include_directories(gxde-ai-see-chattest PRIVATE src)
target_link_libraries(gxde-ai-see-chattest PRIVATE Qt6::Core Qt6::Network)
target_compile_definitions(gxde-ai-see-chattest PRIVATE
    TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(gxde-ai-see-mcptest
    tests/mcptest.cpp
    src/chat/mcpclient.cpp
    src/config.cpp
)
target_include_directories(gxde-ai-see-mcptest PRIVATE src)
target_link_libraries(gxde-ai-see-mcptest PRIVATE Qt6::Core)

# translations
find_program(LRELEASE_EXECUTABLE NAMES lrelease lrelease6)
file(GLOB TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/*.ts)
if (LRELEASE_EXECUTABLE AND TS_FILES)
    set(QM_FILES)
    foreach(ts ${TS_FILES})
        get_filename_component(ts_name ${ts} NAME_WE)
        set(qm ${CMAKE_CURRENT_BINARY_DIR}/translations/${ts_name}.qm)
        add_custom_command(OUTPUT ${qm}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/translations
            COMMAND ${LRELEASE_EXECUTABLE} ${ts} -qm ${qm}
            DEPENDS ${ts}
            VERBATIM)
        list(APPEND QM_FILES ${qm})
    endforeach()
    add_custom_target(gxde-ai-see-translations ALL DEPENDS ${QM_FILES})
    add_dependencies(gxde-ai-see gxde-ai-see-translations)
    install(FILES ${QM_FILES} DESTINATION share/gxde-ai-see/translations)
endif()

install(TARGETS gxde-ai-see RUNTIME DESTINATION bin)
install(FILES data/gxde-ai-see.desktop data/gxde-ai-see-todo.desktop
        data/gxde-ai-see-portal.desktop DESTINATION share/applications)
install(DIRECTORY data/icons/hicolor/ DESTINATION share/icons/hicolor)

include(GNUInstallDirs)
