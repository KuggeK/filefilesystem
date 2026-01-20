cmake_minimum_required(VERSION 3.16)

project(filefilesystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(filefilesystem src/main.cpp
    src/ffsys.hh src/ffsys.cpp
    src/fs_objects.hh
    src/utilities.hh src/utilities.cpp
    src/bitmap.hh src/bitmap.cpp

)

include(GNUInstallDirs)
install(TARGETS filefilesystem
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
