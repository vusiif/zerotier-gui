cmake_minimum_required(VERSION 3.16)
project(zerotier_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
if(MSVC)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "Note: including file: ")
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "Note: including file: ")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${Python3_EXECUTABLE};${CMAKE_CURRENT_SOURCE_DIR}/scripts/msvc_launcher.py")
endif()

# Static Qt builds must use the same static MSVC runtime as their SDK.
get_target_property(QT_CORE_LIBRARY_TYPE Qt6::Core TYPE)
if(MSVC AND QT_CORE_LIBRARY_TYPE STREQUAL "STATIC_LIBRARY")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(SOURCES
    main.cpp
    AppStyle.cpp
    AppLog.cpp
    MainWindow.cpp
    DataTable.cpp
    ZeroTierClient.cpp
    ManagementClient.cpp
    InstallWindow.cpp
    NetworkSettingsDialog.cpp
    NetworkMonitor.cpp
    tabs/ServiceTab.cpp
    tabs/InfoTab.cpp
    tabs/PeersTab.cpp
    tabs/NetworkTab.cpp
    tabs/MoonTab.cpp
)

set(HEADERS
    MainWindow.h
    AppStyle.h
    AppLog.h
    DataTable.h
    ZeroTierClient.h
    ManagementClient.h
    InstallWindow.h
    NetworkSettingsDialog.h
    NetworkMonitor.h
    tabs/ServiceTab.h
    tabs/InfoTab.h
    tabs/PeersTab.h
    tabs/NetworkTab.h
    tabs/MoonTab.h
)

qt_add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)
if(QT_CORE_LIBRARY_TYPE STREQUAL "STATIC_LIBRARY")
    # Keep the headless smoke test available in the standalone executable too.
    qt_import_plugins(${PROJECT_NAME} INCLUDE Qt6::QOffscreenIntegrationPlugin)
endif()
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shell32 advapi32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        # ZeroTier's service token is readable by administrators. Apply this
        # only to the shipped app; regression tests remain unprivileged.
        set_target_properties(${PROJECT_NAME} PROPERTIES
            QT_WINDOWS_APP_PROJECT_EXECUTION_LEVEL requireAdministrator)
        target_link_options(${PROJECT_NAME} PRIVATE /MANIFESTUAC:NO)
    endif()
endif()

option(ZEROTIER_BUILD_TESTS "Build GUI regression tests with a fake ZeroTier CLI" OFF)
if(ZEROTIER_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(fake_zerotier_cli tests/FakeCli.cpp)
    set_target_properties(fake_zerotier_cli PROPERTIES OUTPUT_NAME "zerotier-cli")
    target_link_libraries(fake_zerotier_cli PRIVATE Qt6::Core)
    set(TEST_SOURCES ${SOURCES})
    list(REMOVE_ITEM TEST_SOURCES main.cpp)
    qt_add_executable(zerotier_gui_tests tests/GuiTests.cpp ${TEST_SOURCES} ${HEADERS})
    target_link_libraries(zerotier_gui_tests PRIVATE Qt6::Widgets Qt6::Test)
    if(QT_CORE_LIBRARY_TYPE STREQUAL "STATIC_LIBRARY")
        qt_import_plugins(zerotier_gui_tests INCLUDE Qt6::QOffscreenIntegrationPlugin)
    endif()
    add_dependencies(zerotier_gui_tests fake_zerotier_cli)
    if(WIN32)
        target_link_libraries(zerotier_gui_tests PRIVATE shell32 advapi32)
        target_compile_definitions(zerotier_gui_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    if(MSVC)
        target_compile_options(fake_zerotier_cli PRIVATE /utf-8)
        target_compile_options(zerotier_gui_tests PRIVATE /utf-8)
    endif()
    add_test(NAME gui_regressions COMMAND zerotier_gui_tests
        startupInstallationGate
        windowControlsAndFileLog reconcilePreservesInteraction
        renameSurvivesPollingAndRecreation detailsKeepAllFieldsAndStayNonModal
        mouseDragScrollsBothAxes asynchronousPollAndRecovery windowLayoutAndVisuals
        -platform offscreen -o test-results.txt,txt)
    foreach(test_name IN ITEMS NetworkSettings NetworkMonitor CommandRunner)
        string(TOLOWER "${test_name}" test_target)
        add_executable(${test_target}_test tests/${test_name}Test.cpp
            ManagementClient.cpp ManagementClient.h ZeroTierClient.cpp ZeroTierClient.h AppLog.cpp AppLog.h)
        target_include_directories(${test_target}_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(${test_target}_test PRIVATE Qt6::Widgets)
        if(WIN32)
            target_link_libraries(${test_target}_test PRIVATE advapi32)
        endif()
        if(MSVC)
            target_compile_options(${test_target}_test PRIVATE /utf-8)
        endif()
        add_test(NAME ${test_target} COMMAND ${test_target}_test)
        if(QT_CORE_LIBRARY_TYPE STREQUAL "STATIC_LIBRARY")
            qt_import_plugins(${test_target}_test INCLUDE Qt6::QOffscreenIntegrationPlugin)
        endif()
        set_tests_properties(${test_target} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endforeach()
    target_sources(networksettings_test PRIVATE NetworkSettingsDialog.cpp NetworkSettingsDialog.h)
    target_sources(networkmonitor_test PRIVATE NetworkMonitor.cpp NetworkMonitor.h)
    option(ZEROTIER_TEST_POWERSHELL "Run integration tests that launch PowerShell" OFF)
    if(ZEROTIER_TEST_POWERSHELL)
        add_test(NAME powershell_regressions COMMAND zerotier_gui_tests
            uninstallOnlyCleansAfterSuccess powerShellRedirectionDoesNotBlockPayload
            -platform offscreen -o powershell-test-results.txt,txt)
    endif()
endif()

# Size and memory optimizations for Release
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8
        $<$<CONFIG:Release>:/O1 /GL /Gy>)
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>)
endif()
