cmake_minimum_required(VERSION 4.3)
project(GameboyEmu C)

set(CMAKE_C_STANDARD 23)

add_executable(GameboyEmu
        src/cpu.c
        src/cpu.h
        include/types.h
        src/memory.c
        src/memory.h)
