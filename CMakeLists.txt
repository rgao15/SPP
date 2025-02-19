# Copyright (c) David Sleeper (Sleeping Robot LLC)

# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
# to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, 
# and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
# DEALINGS IN THE SOFTWARE.

cmake_minimum_required(VERSION 3.5)
project(SPPGLOBAL VERSION 1.0.0 LANGUAGES CXX C)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY OS_FOLDERS ON)

LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

message( STATUS "BUILD SYSTEM: ${CMAKE_SYSTEM}" )
message( STATUS "BUILD SYSTEM NAME: ${CMAKE_SYSTEM_NAME}" ) 
message( STATUS "BUILD SYSTEM PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}" )

include(wxWidgetsHelper)

SET(pre_configure_dir "${CMAKE_CURRENT_SOURCE_DIR}/CONFIGURE" )
SET(post_configure_dir "${CMAKE_CURRENT_SOURCE_DIR}/CONFIGURE" )

include(CheckGit)
CheckGitSetup()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set(OS_MAC ON)
	message(STATUS "MACOS FOUND")	
	add_definitions(-DPLATFORM_MAC=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(OS_LINUX ON)
	message(STATUS "LINUX FOUND")
	add_definitions(-DPLATFORM_LINUX=1)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(OS_WINDOWS ON)
	message(STATUS "WINDOWS FOUND")
	add_definitions(-DPLATFORM_WINDOWS=1)
else()
	message(FATAL_ERROR "We do not know OS!!!")
endif()
	
set(SPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" )
set(THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/3rdParty" )	
set(CMAKE_DEBUG_POSTFIX "d" )	
	
if(OS_WINDOWS)
	set(LIB_PATH_POSTFIX "/win64/vc16" )	
else()
	set(LIB_PATH_POSTFIX "" )	
endif()

set(USE_CEF OFF CACHE BOOL "Use CEF 3rd Party")
set(USE_GRAPHICS OFF CACHE BOOL "Use SPP Graphics projects")
set(USE_DX12 OFF CACHE BOOL "Use DX12")
set(USE_OPENGL OFF CACHE BOOL "Use OpenGL")
set(USE_VULKAN OFF CACHE BOOL "Use Vulkan")

set(USE_CRYPTOCPP ON CACHE BOOL "Include lib Cryptopp")
set(USE_CRYPTOCPP_NETCONN ON CACHE BOOL "Use Cryptopp in networking")

set(USE_CUDA OFF CACHE BOOL "Use CUDA when applicable")
set(USE_OBJECTS ON CACHE BOOL "Use Objects (includes GC, RTTR, ...)")
set(USE_WINRT ON CACHE BOOL "Use C++/WinRt")

if(OS_WINDOWS)
	set(WIN_SDK_REG "HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\Microsoft SDKs\\Windows\\v10.0")

	GET_FILENAME_COMPONENT(WIN10_SDK "[${WIN_SDK_REG};InstallationFolder]" ABSOLUTE CACHE)
	GET_FILENAME_COMPONENT(WIN10_SDK_VER "[${WIN_SDK_REG};ProductVersion]" ABSOLUTE CACHE)
	message(STATUS "WIN10_SDK = ${WIN10_SDK}")
	message(STATUS "WIN10_SDK_VER = ${WIN10_SDK_VER}")

	if(USE_WINRT)
		add_definitions(-DHAS_WINRT=1)
	endif()
endif()

if(USE_OBJECTS)
	add_definitions(-DUSE_OBJECTS=1)
endif()

#General C++ Compiling
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)

	set(USE_VS_MULTIPROCESS ON CACHE BOOL "Use MultiProcessor compiling on VS")
	
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
	
	if(USE_VS_MULTIPROCESS)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
	endif()
	
else()
	
	# set position independent
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fPIC")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
 
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -D_DEBUG")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG -D_DEBUG")

    if(OS_LINUX)
        link_libraries(stdc++fs dl pthread)
    endif()
    
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_SOURCE_DIR}/Binaries/ReleaseWithDebug/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Release/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/Binaries/Debug/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)

set(SPP_TARGET_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Binaries)

include_directories( "${THIRD_PARTY}" )

list(APPEND 
		SPPProjects 
			"SPPCore" 
			"SPPDatabase" 
			"SPPNetworking" 
			"SPPMath"			
			 )

if(USE_CRYPTOCPP)

	list(APPEND 
			SPPProjects 
				"SPPCrypto" 
				 )

endif()

list(APPEND SPPProjects
			"SPPEngine" )

if(OS_WINDOWS)
	list(APPEND SPPProjects			
				"SPPCapture" 
				"SPPVideo" )
endif()		

if(USE_OBJECTS)
	list(APPEND SPPProjects								
			"SPPReflection"
			"SPPObject")
endif()
			
if(OS_WINDOWS)
	
	if(USE_CEF)
		list(APPEND SPPProjects "SPPCEF")
	endif()

	if(USE_GRAPHICS)
		list(APPEND SPPProjects "SPPMesh" "SPPGraphics")
		
		if(USE_OBJECTS)
			list(APPEND SPPProjects "SPPGraphicsO")
		endif()
		
		if(USE_DX12)
			list(APPEND SPPProjects "SPPDX12")	
			list(APPEND SPPGDI "SPPDX12")
		endif()	
		
		if(USE_OPENGL)
			list(APPEND SPPProjects "SPPOpenGL")	
			list(APPEND SPPGDI "SPPOpenGL")
		endif()
		
		if(USE_VULKAN)
			list(APPEND SPPProjects "SPPVulkan")	
			list(APPEND SPPGDI "SPPVulkan")
		endif()

		if(USE_OBJECTS)
			list(APPEND SPPProjects		
				"SPPSceneO"				
				"SPPSDFO")
		endif()
	endif()
	
	if(USE_WINRT)
		list(APPEND SPPProjects "Misc/SPPWinRTBTE")
	endif()
	
endif()

message( STATUS "SPP GDI LIST: ${SPPGDI}" )

list(APPEND SPPProjects "applications" ) 

if(OS_WINDOWS)
	list(APPEND SPPProjects "tests" ) 
endif()


#"tests" "assets" )

# swapped to wxwidgets	
# if(OS_WINDOWS)
# 	list(APPEND SPPProjects "RemoteControllerWPF" "RemoteApplicationWPF" "ApplicationThroughputWPF")
# endif()

# set working directory
function(add_executable target_name)
	# Call the original function
	_add_executable(${target_name} ${ARGN})
	message( STATUS "add_executable ${target_name}" )
	if (WIN32)
		set_property(TARGET ${target_name} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${SPP_ROOT}/Binaries")
	endif()
endfunction(add_executable target_name)


foreach(subdir ${SPPProjects})	
	message( STATUS "Adding Project: ${subdir}" )
    add_subdirectory(${subdir})
endforeach()

