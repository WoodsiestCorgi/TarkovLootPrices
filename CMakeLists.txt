cmake_minimum_required(VERSION 3.24)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(TarkovLootPrices VERSION 0.2.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "TarkovLootPrices targets Windows 10/11 only")
endif()

include(FetchContent)
FetchContent_Declare(nlohmann_json URL
  https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(TarkovLootPrices WIN32 src/main.cpp)
target_compile_features(TarkovLootPrices PRIVATE cxx_std_20)
target_compile_definitions(TarkovLootPrices PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(TarkovLootPrices PRIVATE user32 gdi32 gdiplus winhttp shell32 nlohmann_json::nlohmann_json)
if(MSVC)
    target_compile_options(TarkovLootPrices PRIVATE /W4 /permissive- /utf-8 /EHsc)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(price_math_tests tests/price_math_tests.cpp)
    target_compile_features(price_math_tests PRIVATE cxx_std_20)
    target_include_directories(price_math_tests PRIVATE src)
    add_test(NAME price_math COMMAND price_math_tests)
    add_executable(catalog_tests tests/catalog_tests.cpp)
    target_compile_features(catalog_tests PRIVATE cxx_std_20)
    target_include_directories(catalog_tests PRIVATE src)
    target_link_libraries(catalog_tests PRIVATE nlohmann_json::nlohmann_json)
    add_test(NAME catalog COMMAND catalog_tests)
endif()
