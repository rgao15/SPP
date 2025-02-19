cmake_minimum_required(VERSION 3.5)
project(SPPCrypto_API VERSION 1.0.0 LANGUAGES CXX)

add_library(SPPCrypto SHARED "")

set_target_properties(SPPCrypto PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})

add_dependencies( SPPCrypto SPPCore )

# will need define to not create empty dll export
target_compile_definitions(SPPCrypto
	PRIVATE
		SPP_CRYPTO_EXPORT=1)
				
target_sources(SPPCrypto
	PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/SPPCrypto.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/SPPCrypto.h"				
		)

target_include_directories(SPPCrypto 
	PUBLIC  	
		"${CMAKE_CURRENT_LIST_DIR}" )

target_include_directories(SPPCrypto 
	PRIVATE  	
		"${THIRD_PARTY}/cryptopp/include" )
			
target_link_libraries( SPPCrypto 
	PUBLIC
		SPPCore )

target_link_directories(SPPCrypto
    PUBLIC
        "${THIRD_PARTY}/cryptopp/lib"
        )

if(OS_WINDOWS)

target_link_libraries( SPPCrypto 
	PRIVATE
		debug "Debug/cryptlib" optimized "Release/cryptlib" )

else()

target_link_libraries( SPPCrypto 
	PRIVATE
		debug "cryptoppd" optimized "cryptopp" )

endif()

install(TARGETS SPPCrypto DESTINATION "${SPP_ROOT}/INSTALL")

if(OS_LINUX)

install(DIRECTORY "${THIRD_PARTY}/cryptopp/lib/" #trailing / matters to flatten it
	DESTINATION "${SPP_ROOT}/INSTALL"
	FILES_MATCHING 
		PATTERN "*.so.*"
		PATTERN mswu EXCLUDE 
		PATTERN mswud EXCLUDE)

endif()

