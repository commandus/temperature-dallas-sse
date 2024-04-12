cmake_minimum_required(VERSION 3.9)
set(PROJECT_NAME "temperature-dallas-sse")
project(${PROJECT_NAME} VERSION 1.0)

set(CMAKE_CXX_STANDARD 11)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	# avoid Error LNK2038 mismatch detected for 'RuntimeLibrary': value 'MT_StaticRelease' doesn't match value 'MD_DynamicRelease'
	# set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
	# set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
	set(OS_SPECIFIC_LIBS wsock32 ws2_32 Userenv d3d12 d3dcompiler dxgi)
else()
	set(OS_SPECIFIC_LIBS pthread)	
endif()

add_executable(temperature-dallas-sse
	temperature-dallas-sse.cpp
)
target_link_libraries(temperature-dallas-sse PRIVATE ${OS_SPECIFIC_LIBS})
target_include_directories(temperature-dallas-sse PRIVATE "." "third-party")
