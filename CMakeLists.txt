cmake_minimum_required(VERSION 3.16)
project(SOFTUDIO LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "C:/Qt/6.9.0/mingw_64") # Verify this Qt path

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(SOFTUDIO
    # Main application file
    main.cpp

    # Splash screen and related UI components
    splash_constants.h
    animatedloadinglabel.h
    animatedloadinglabel.cpp
    shiningbutton.h
    shiningbutton.cpp
    loadingworker.h
    loadingworker.cpp
    splashscreen.h
    splashscreen.cpp
    framelessdialogbase.h   # Included as ScannerDialog uses it
    framelessdialogbase.cpp # CRITICAL: Add the .cpp file

    # New Scanner components
    projectinfo.h           # Header only, but good to list for clarity
    scannerdialog.h
    scannerdialog.cpp
    scanworker.h
    scanworker.cpp
    projectfilevalidatorworker.h
    projectfilevalidatorworker.cpp

    # Resources
    app_resources.rc
)

# Ensure all necessary Qt components are linked.
# Qt6::Widgets should bring in Core and Gui, but explicitly adding them doesn't hurt.
target_link_libraries(SOFTUDIO PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

if(WIN32)
    set_target_properties(SOFTUDIO PROPERTIES WIN32_EXECUTABLE ON)
endif()