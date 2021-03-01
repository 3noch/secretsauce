cmake_minimum_required(VERSION 3.1)
project(reflex-cpp)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_SRCS
)

set(PROJECT_INCS
  ${PROJECT_SOURCE_DIR}/include/Containers.hpp
  ${PROJECT_SOURCE_DIR}/include/ThreadSafe.hpp
  ${PROJECT_SOURCE_DIR}/include/FRP.hpp
)

source_group("include" FILES ${PROJECT_INCS})
source_group("src"     FILES ${PROJECT_SRCS})

find_package(Boost COMPONENTS)

add_library(${PROJECT_NAME} INTERFACE include/)
target_sources(${PROJECT_NAME} INTERFACE "$<BUILD_INTERFACE:${PROJECT_INCS}>")
target_include_directories(${PROJECT_NAME} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>)
target_include_directories(${PROJECT_NAME} SYSTEM INTERFACE $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_11)

install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

add_executable(reflex-cpp-example
  ${PROJECT_SOURCE_DIR}/Example.cpp
)
target_link_libraries(reflex-cpp-example ${PROJECT_NAME})
