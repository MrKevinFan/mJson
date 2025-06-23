cmake_minimum_required(VERSION 3.19)
project(mJson VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")

set(MJSON_SOURCES
  src/mJson.c
)

# 主库
add_library(mJson SHARED)
# 主程序
#add_executable(mJson)
target_sources(mJson PRIVATE ${MJSON_SOURCES})
target_include_directories(mJson PRIVATE include src)

# 示例程序
add_executable(example example/main.c)
target_include_directories(example PRIVATE include src)
target_link_libraries(example mJson)