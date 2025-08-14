add_executable(LoggerTests main.cpp)
target_include_directories(LoggerTests PRIVATE ${CMAKE_SOURCE_DIR}/logger/include)
target_link_libraries(LoggerTests PRIVATE LoggerStatic) # или LoggerShared
