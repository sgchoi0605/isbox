cmake_minimum_required(VERSION 3.20)

project(isbox_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)
find_package(unofficial-libmariadb CONFIG REQUIRED)

add_executable(isbox_backend
    backend/main.cpp
    backend/controllers/DbTestController.cpp
)

target_include_directories(isbox_backend PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
)

target_compile_options(isbox_backend PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

target_link_libraries(isbox_backend PRIVATE
    Drogon::Drogon
    unofficial::libmariadb
)
