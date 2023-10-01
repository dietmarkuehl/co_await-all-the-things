cmake_minimum_required(VERSION 3.12.0)
project(accu-2023 VERSION 1.0.0)
include(CheckIncludeFile)

set(CMAKE_CXX_STANDARD 20)
set(SANITIZE_RECOVER -fno-sanitize-recover)
set(SANITIZE -fsanitize=address)
add_compile_options(${SANITIZE})
add_compile_options(${SANITIZE_RECOVER})
add_link_options(${SANITIZE})

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -W -Wall -Wno-missing-field-initializers -fconcepts-diagnostics-depth=4 -ftemplate-backtrace-limit=0")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -W -Wall -Werror -stdlib=libc++")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNOMINMAX")
endif()


list(APPEND DEMOS
    task-accu
    task-using-std
)

foreach(demo ${DEMOS})
    add_executable(${demo} ${demo}.cpp)
endforeach()
