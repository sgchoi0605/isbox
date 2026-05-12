cmake_minimum_required(VERSION 3.21)

project(isbox_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)

add_executable(isbox_backend
    app/main.cpp
    controllers/MemberController.cpp
    # 게시판 기능은 컨트롤러-서비스-매퍼 계층으로 나뉘며 모두 같은 백엔드 바이너리에 포함된다.
    controllers/BoardController.cpp
    controllers/IngredientController.cpp
    services/MemberService.cpp
    services/BoardService.cpp
    services/IngredientService.cpp
    mappers/MemberMapper.cpp
    mappers/BoardMapper.cpp
    mappers/IngredientMapper.cpp
)

target_link_libraries(isbox_backend PRIVATE Drogon::Drogon)

if(MSVC)
    target_compile_options(isbox_backend PRIVATE /utf-8)
endif()
