cmake_minimum_required(VERSION 3.10)
project(LoggerProject)

set(CMAKE_CXX_STANDARD 17)

#подкаталоги
add_subdirectory(app_stats)
add_subdirectory(logger)
add_subdirectory(app)
add_subdirectory(tests)
