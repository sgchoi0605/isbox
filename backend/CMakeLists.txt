cmake_minimum_required(VERSION 3.21)

project(isbox_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon CONFIG REQUIRED)

add_executable(isbox_backend
    app/main.cpp
    member/controller/MemberController.cpp
    friend/controller/FriendController.cpp
    board/controller/BoardController.cpp
    ingredient/controller/IngredientController.cpp
    member/service/MemberService.cpp
    member/service/MemberService_Auth.cpp
    member/service/MemberService_Profile.cpp
    member/service/MemberService_Profile_Query.cpp
    member/service/MemberService_Profile_Command.cpp
    member/service/MemberService_Experience.cpp
    member/service/MemberService_SessionStore.cpp
    friend/service/FriendService.cpp
    friend/service/FriendService_Query.cpp
    friend/service/FriendService_Command.cpp
    friend/service/FriendService_Policy.cpp
    board/service/BoardService.cpp
    board/service/BoardService_Query.cpp
    board/service/BoardService_Command.cpp
    board/service/BoardService_Validation.cpp
    ingredient/service/IngredientService.cpp
    ingredient/service/IngredientService_Query.cpp
    ingredient/service/IngredientService_Command.cpp
    ingredient/service/IngredientService_Validation.cpp
    ingredient/service/ProcessedFoodSearchService.cpp
    ingredient/service/NutritionFoodsSearchService.cpp
    ingredient/service/ProcessedFoodSearchService_Client.cpp
    ingredient/service/ProcessedFoodSearchService_Assembler.cpp
    ingredient/service/ProcessedFoodSearchService_Cache.cpp
    ingredient/service/ProcessedFoodSearchService_KeyProvider.cpp
    ingredient/service/ProcessedFoodSearchService_Text.cpp
    member/mapper/MemberMapper.cpp
    friend/mapper/FriendMapper.cpp
    board/mapper/BoardMapper.cpp
    ingredient/mapper/IngredientMapper.cpp
)

target_link_libraries(isbox_backend PRIVATE Drogon::Drogon)
target_include_directories(isbox_backend PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(isbox_backend PRIVATE /utf-8)
endif()
