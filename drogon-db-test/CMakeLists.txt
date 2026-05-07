cmake_minimum_required(VERSION 3.21)
# 이 프로젝트를 빌드하려면 최소 CMake 3.21 이상이 필요하다.

project(drogon_db_test LANGUAGES CXX)
# 프로젝트 이름은 drogon_db_test, 사용 언어는 C++로 선언한다.

set(CMAKE_CXX_STANDARD 17)
# C++17 표준을 사용한다.

set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 컴파일러가 C++17을 지원하지 않으면 빌드를 실패시킨다.

set(CMAKE_CXX_EXTENSIONS OFF)
# GNU 확장 같은 비표준 확장을 끄고 표준 C++ 문법만 사용한다.

find_package(Drogon CONFIG REQUIRED)
# Drogon 패키지를 CMake Config 방식으로 찾는다. 못 찾으면 즉시 실패한다.

add_executable(drogon_db_test
    src/main.cpp
)
# 실행 파일 타깃 drogon_db_test를 만들고 엔트리 소스(src/main.cpp)를 연결한다.

target_link_libraries(drogon_db_test PRIVATE Drogon::Drogon)
# drogon_db_test 타깃에 Drogon 라이브러리를 링크한다.

set(MYSQL_ROOT "C:/Program Files/MySQL/MySQL Server 8.0")
# Windows 로컬에 설치된 MySQL 루트 경로를 변수로 저장한다.

target_include_directories(drogon_db_test PRIVATE "${MYSQL_ROOT}/include")
# MySQL 헤더(mysql.h 등)를 찾기 위해 include 디렉터리를 추가한다.

target_link_libraries(drogon_db_test PRIVATE "${MYSQL_ROOT}/lib/libmysql.lib")
# MySQL 클라이언트 import 라이브러리(libmysql.lib)를 링크한다.

add_custom_command(TARGET drogon_db_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MYSQL_ROOT}/lib/libmysql.dll"
    "$<TARGET_FILE_DIR:drogon_db_test>/libmysql.dll")
# 빌드 후 실행 파일 폴더로 libmysql.dll을 복사한다.
# 이렇게 하면 PATH를 따로 잡지 않아도 실행 시 DLL 로딩 오류를 줄일 수 있다.
