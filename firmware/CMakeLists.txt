cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pix_in_the_loop)

zephyr_include_directories(include)

add_subdirectory(src)
