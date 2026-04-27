cmake_minimum_required(VERSION 3.10)
project(os_project C)

set(CMAKE_C_STANDARD 11)

add_executable(os_project
        main.c
        graph.c
        dijkstra.c
)