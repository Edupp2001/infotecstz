set(LOGGER_SRC
    src/Logger.cpp
)

set(LOGGER_HEADERS
    include/Logger/Logger.h
)

add_library(LoggerStatic STATIC ${LOGGER_SRC} ${LOGGER_HEADERS})
target_include_directories(LoggerStatic PUBLIC include)
