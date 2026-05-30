cmake_minimum_required(VERSION 3.16)
project(zerotier_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC OFF)

# Qt path
list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.8.3/msvc2022_64")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(SOURCES
    main.cpp
    MainWindow.cpp
    tabs/ServiceTab.cpp
    tabs/InfoTab.cpp
    tabs/PeersTab.cpp
    tabs/NetworkTab.cpp
    tabs/MoonTab.cpp
)

set(HEADERS
    MainWindow.h
    tabs/ServiceTab.h
    tabs/InfoTab.h
    tabs/PeersTab.h
    tabs/NetworkTab.h
    tabs/MoonTab.h
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)

# Size and memory optimizations for Release
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:/O1 /GL /Gy>
)
target_link_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
)
