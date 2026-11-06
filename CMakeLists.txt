cmake_minimum_required(VERSION 3.20)

project(gxde-ai-see
    VERSION 1.0.0
    DESCRIPTION "GXDE AI See - 小埃同学"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network DBus Svg Qml Quick QuickControls2)
find_package(Qt6 OPTIONAL_COMPONENTS WaylandClient WaylandScannerTools)
find_package(GxdeBerneseQt6 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(XCB QUIET xcb)
pkg_check_modules(XCB_XFIXES QUIET xcb-xfixes)
pkg_check_modules(DTK2WIDGET REQUIRED dtk2widget)
pkg_check_modules(DTK6GUI REQUIRED dtk6gui)
pkg_check_modules(ICU REQUIRED icu-i18n icu-uc)
find_package(LayerShellQt QUIET)

set(GXDE_AI_SEE_SOURCES
    src/main.cpp
    src/app.cpp
    src/config.cpp
    src/staging/stagingmodel.cpp
    src/portal/portalclient.cpp
    src/portal/portalsession.cpp
    src/portal/taskwindows.cpp
    src/factory/linktools.cpp
    src/factory/contentclassifier.cpp
    src/factory/factoryactions.cpp
    src/dialogs/flowlayout.cpp
    src/dialogs/segmentdialog.cpp
    src/dialogs/speechdialog.cpp
    src/actions/downloader.cpp
    src/actions/texttools.cpp
    src/todo/todomodel.cpp
    src/todo/todowindow.cpp
    src/chat/aicore.cpp
    src/chat/aicoreaccounts.cpp
    src/chat/chatclient.cpp
    src/chat/chatwindow.cpp
    src/chat/chattheme.cpp
    src/chat/markdown.cpp
    src/chat/chatview.cpp
    src/ui/mimedroparea.cpp
    src/ui/pointertravel.cpp
    src/portal/dragwatcher.cpp
    src/ui/portalpainters.cpp
    src/ui/portalpanel.cpp
    src/ui/aboutdialog.cpp
)

add_executable(gxde-ai-see ${GXDE_AI_SEE_SOURCES})

# The portal panel is Qt Quick, styled by GXTK7 (QtQuick.Controls.Gxde).
qt_policy(SET QTP0001 NEW)
set(GXDE_AI_SEE_QML_FILES
    src/qml/PortalPanel.qml
    src/qml/PortalZone.qml
    src/qml/StagingList.qml
    src/qml/ContentFactory.qml
    src/qml/DropButton.qml
    src/qml/WindowGrid.qml
)
foreach(file IN LISTS GXDE_AI_SEE_QML_FILES)
    get_filename_component(alias "${file}" NAME)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS "${alias}")
endforeach()
qt_add_qml_module(gxde-ai-see
    URI GxdeAiSee
    VERSION 1.0
    QML_FILES ${GXDE_AI_SEE_QML_FILES}
    NO_PLUGIN
)

# Taskbar style window access for the portal on X11 / treeland / wlroots.
if (XCB_FOUND)
    target_sources(gxde-ai-see PRIVATE src/portal/x11taskwindows.cpp)
    target_include_directories(gxde-ai-see PRIVATE ${XCB_INCLUDE_DIRS})
    target_link_libraries(gxde-ai-see PRIVATE ${XCB_LIBRARIES})
    target_compile_definitions(gxde-ai-see PRIVATE GXDE_HAVE_XCB=1)

    if (XCB_XFIXES_FOUND)
        target_include_directories(gxde-ai-see PRIVATE ${XCB_XFIXES_INCLUDE_DIRS})
        target_link_libraries(gxde-ai-see PRIVATE ${XCB_XFIXES_LIBRARIES})
        target_compile_definitions(gxde-ai-see PRIVATE GXDE_HAVE_XCB_XFIXES=1)
    endif ()
endif ()
if (TARGET Qt6::WaylandClient AND COMMAND qt6_generate_wayland_protocol_client_sources)
    target_sources(gxde-ai-see PRIVATE src/portal/wlrtaskwindows.cpp)
    qt6_generate_wayland_protocol_client_sources(gxde-ai-see
        PRIVATE_CODE
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/portal/protocols/wlr-foreign-toplevel-management-unstable-v1.xml
              ${CMAKE_CURRENT_SOURCE_DIR}/src/portal/protocols/relative-pointer-unstable-v1.xml
              ${CMAKE_CURRENT_SOURCE_DIR}/src/portal/protocols/treeland-dde-shell-v1.xml)
    target_link_libraries(gxde-ai-see PRIVATE Qt6::WaylandClient)
    target_compile_definitions(gxde-ai-see PRIVATE GXDE_HAVE_WAYLAND_CLIENT=1)
endif ()

target_include_directories(gxde-ai-see PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # the generated QML type registration includes headers by file name only
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/portal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/staging
    ${DTK2WIDGET_INCLUDE_DIRS}
    ${DTK6GUI_INCLUDE_DIRS}
    ${ICU_INCLUDE_DIRS}
)

target_link_libraries(gxde-ai-see PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::DBus
    Qt6::Svg
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    GxdeBernese::Qt6
    ${DTK2WIDGET_LIBRARIES}
    ${DTK6GUI_LIBRARIES}
    ${ICU_LIBRARIES}
)

target_link_directories(gxde-ai-see PRIVATE
    ${DTK2WIDGET_LIBRARY_DIRS}
    ${DTK6GUI_LIBRARY_DIRS}
)

if (LayerShellQt_FOUND)
    target_link_libraries(gxde-ai-see PRIVATE LayerShellQt::Interface)
    target_compile_definitions(gxde-ai-see PRIVATE GXDE_HAVE_LAYERSHELLQT=1)
endif ()

add_executable(gxde-ai-see-selftest
    tests/selftest.cpp
    src/chat/chattheme.cpp
    src/factory/linktools.cpp
    src/factory/contentclassifier.cpp
    src/factory/factoryactions.cpp
    src/actions/texttools.cpp
)
target_include_directories(gxde-ai-see-selftest PRIVATE src ${ICU_INCLUDE_DIRS})
target_link_libraries(gxde-ai-see-selftest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets ${ICU_LIBRARIES})

add_executable(gxde-ai-see-aicoretest
    tests/aicoretest.cpp
    src/chat/aicore.cpp
    src/chat/chatclient.cpp
    src/config.cpp
)

target_include_directories(gxde-ai-see-aicoretest PRIVATE src src/chat)
target_link_libraries(gxde-ai-see-aicoretest PRIVATE Qt6::Core)
target_compile_definitions(gxde-ai-see-aicoretest PRIVATE
    TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_executable(gxde-ai-see-themetest
    tests/themetest.cpp
    src/chat/chattheme.cpp
)
target_include_directories(gxde-ai-see-themetest PRIVATE src)
target_link_libraries(gxde-ai-see-themetest PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

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
