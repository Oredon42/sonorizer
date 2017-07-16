set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

project(Sonorizer)

cmake_minimum_required(VERSION 2.8)
set(SOURCE_DIR src)
set(INCLUDE_DIR include)

file(GLOB_RECURSE SOURCES ${SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE HEADERS ${INCLUDE_DIR}/*.h)

set (INCLUDE_DIRS "")
foreach (_headerFile ${HEADERS})
    get_filename_component(_dir ${_headerFile} PATH)
    list (APPEND INCLUDE_DIRS ${_dir})
endforeach()
list(REMOVE_DUPLICATES INCLUDE_DIRS)

include_directories(${INCLUDE_DIRS})

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(APPLE)
        set(CMAKE_CXX_FLAGS "-std=c++11 -g -ggdb -O0 -Wall -Wextra")

        #OpenAL inclusion
        find_package(OpenAL)
        include_directories(${OPENAL_INCLUDE_DIR})

        #OpenCV inclusion
        find_package(OpenCV REQUIRED)
        include_directories(${OpenCV_INCLUDE_DIRS})

        add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

        target_link_libraries(${PROJECT_NAME} ${OPENAL_LIBRARY} ${OpenCV_LIBS})
endif()
