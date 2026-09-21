cmake_minimum_required(VERSION 3.16)

project(EveCompanion VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Sql NetworkAuth HttpServer)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Sql NetworkAuth HttpServer)
add_subdirectory(../RedPandasToolbox RedPandasToolbox_build)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(EveCompanion
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        core.h core.cpp
        esimanager.h esimanager.cpp
        itemsviewform.h itemsviewform.cpp itemsviewform.ui
        settings.h
        industryform.h industryform.cpp industryform.ui
        itemwidget.h itemwidget.cpp
        rsc.qrc
        subapplications.h subapplications.cpp
        addstationform.h addstationform.cpp addstationform.ui
        loadingform.h loadingform.cpp loadingform.ui
        characterconnexionform.h characterconnexionform.cpp characterconnexionform.ui
        productionchainform.h productionchainform.cpp productionchainform.ui
        productionnodes.h productionnodes.cpp
        itemfluxwidget.h itemfluxwidget.cpp
        itemstreamsmanagerform.h itemstreamsmanagerform.cpp itemstreamsmanagerform.ui
        standingmanagerform.h standingmanagerform.cpp standingmanagerform.ui
        settingsform.h settingsform.cpp settingsform.ui
        zkbsmartuploaderform.h zkbsmartuploaderform.cpp zkbsmartuploaderform.ui
        killmailwidget.h killmailwidget.cpp
        redpandasmarket.h redpandasmarket.cpp
        redpandasmarketform.h redpandasmarketform.cpp redpandasmarketform.ui
        redpandasgarden.h redpandasgarden.cpp
        asyncmanager.h asyncmanager.cpp



    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET EveCompanion APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(EveCompanion SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(EveCompanion
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(EveCompanion PRIVATE Qt${QT_VERSION_MAJOR}::Widgets RedPandasToolbox dwmapi Qt6::Network Qt6::HttpServer Qt6::Sql Qt6::NetworkAuth)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.EveCompanion)
endif()
set_target_properties(EveCompanion PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

add_custom_command(TARGET EveCompanion POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:RedPandasToolbox>
        $<TARGET_FILE_DIR:EveCompanion>
)

include(GNUInstallDirs)
install(TARGETS EveCompanion
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(EveCompanion)
endif()
