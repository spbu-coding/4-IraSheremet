cmake_minimum_required(VERSION 3.7)
project(tests LANGUAGES C)

set(CMAKE_C_STANDARD 99)

add_executable(converter src/qdbmp_negative.c src/qdbmp.c src/custom_bmp.c src/converter.c)
add_executable(comparer src/custom_bmp.c src/comparer.c)



