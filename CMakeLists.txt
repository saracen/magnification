# CMake version check
cmake_minimum_required(VERSION 3.10)

# Project Name
project(magnification)

# Dependent packages
find_package(OpenCV REQUIRED core imgproc highgui)

# Includes
include_directories( ${OpenCV_INCLUDE_DIRS} )

# Executable file directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# Executable
add_executable(${PROJECT_NAME}
	src/main.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
	${OpenCV_LIBS}
)
