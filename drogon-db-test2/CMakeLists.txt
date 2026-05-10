cmake_minimum_required(VERSION 3.21)
# 이 프로젝트는 CMake 3.21 이상이 필요하다.

project(drogon_db_test2 LANGUAGES CXX)
# 프로젝트 이름은 drogon_db_test2이며 C++ 프로젝트다.

set(CMAKE_CXX_STANDARD 17)
# C++17 표준을 사용한다.

set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 컴파일러가 C++17을 지원하지 않으면 구성 단계에서 실패시킨다.

set(CMAKE_CXX_EXTENSIONS OFF)
# 이식성을 위해 컴파일러 고유 확장 문법을 비활성화한다.

find_package(Drogon CONFIG REQUIRED)
# CMake 패키지 설정 파일을 통해 Drogon을 찾는다.

add_executable(drogon_db_test2
    src/main.cpp
)
# 엔트리 소스(src/main.cpp)로 실행 파일을 빌드한다.

target_link_libraries(drogon_db_test2 PRIVATE Drogon::Drogon)
# 실행 파일에 Drogon 라이브러리를 링크한다.

if(MSVC)
    target_compile_options(drogon_db_test2 PRIVATE /utf-8)
endif()
# MSVC가 소스 파일을 UTF-8로 해석하도록 설정한다.

# DB 접근은 config.json의 db_clients 설정을 통해 Drogon이 관리한다.
# 앱 코드에서 mysql 헤더를 직접 include하거나 mysql 라이브러리를 수동 링크하지 않는다.
