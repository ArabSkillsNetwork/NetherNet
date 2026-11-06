cmake_minimum_required(VERSION 3.22)
project(php_nethernet C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

if(WIN32)
	set(PHP_DEV_DIR "C:/php-sdk/php-devpack" CACHE PATH "PHP ZTS devpack")
	set(PHP_INCLUDE_DIR "${PHP_DEV_DIR}/include")
	set(PHP_BUILD_ID "API20220829,TS,VS17" CACHE STRING "Module build ID of the target php binary")

	set(OPENSSL_USE_STATIC_LIBS TRUE)
	set(OPENSSL_MSVC_STATIC_RT FALSE)
else()
	set(PHP_INCLUDE_DIR "" CACHE PATH "PHP headers (include/php)")
	if(NOT EXISTS "${PHP_INCLUDE_DIR}/main/php.h")
		message(FATAL_ERROR "PHP_INCLUDE_DIR must point at include/php of a PHP ZTS build, got \"${PHP_INCLUDE_DIR}\"")
	endif()
	option(NN_STATIC_CXX "Link libstdc++ and libgcc statically" OFF)

	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

set(CNETHERNET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../CNetherNet" CACHE PATH "CNetherNet source checkout")
if(NOT EXISTS "${CNETHERNET_DIR}/nethernet/nn_peer.h")
	message(FATAL_ERROR "CNETHERNET_DIR must point at a CNetherNet checkout, got \"${CNETHERNET_DIR}\"")
endif()
add_subdirectory(${CNETHERNET_DIR} nethernet EXCLUDE_FROM_ALL)

find_package(OpenSSL REQUIRED)

add_library(php_nethernet SHARED php_nethernet.c)

target_include_directories(php_nethernet PRIVATE
	${PHP_INCLUDE_DIR}
	${PHP_INCLUDE_DIR}/main
	${PHP_INCLUDE_DIR}/TSRM
	${PHP_INCLUDE_DIR}/Zend
	${PHP_INCLUDE_DIR}/ext
)

target_link_libraries(php_nethernet PRIVATE
	nethernet
	datachannel-static
	OpenSSL::SSL OpenSSL::Crypto
)

if(WIN32)
	set_target_properties(php_nethernet PROPERTIES PREFIX "" OUTPUT_NAME "php_nethernet")

	target_compile_definitions(php_nethernet PRIVATE
		WIN32 _WINDOWS ZEND_WIN32 PHP_WIN32 ZTS=1 ZEND_DEBUG=0 COMPILE_DL_NETHERNET
		_CRT_SECURE_NO_WARNINGS
		NN_PHP_BUILD_ID="${PHP_BUILD_ID}"
	)
	target_compile_options(php_nethernet PRIVATE /experimental:c11atomics)

	target_link_libraries(php_nethernet PRIVATE
		${PHP_DEV_DIR}/lib/php8ts.lib
		ws2_32 crypt32 bcrypt iphlpapi
	)

	if(PHP_BIN_DIR)
		add_custom_command(TARGET php_nethernet POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:php_nethernet> ${PHP_BIN_DIR}/ext/php_nethernet.dll)
	endif()
else()
	set_target_properties(php_nethernet PROPERTIES PREFIX "" OUTPUT_NAME "nethernet" C_VISIBILITY_PRESET hidden)

	target_compile_definitions(php_nethernet PRIVATE COMPILE_DL_NETHERNET)

	target_link_options(php_nethernet PRIVATE -Wl,--exclude-libs,ALL)
	target_link_options(php_nethernet PRIVATE -Wl,-z,nodelete)
	if(NN_STATIC_CXX)
		target_link_options(php_nethernet PRIVATE -static-libstdc++ -static-libgcc)
	endif()
	find_package(Threads REQUIRED)
	target_link_libraries(php_nethernet PRIVATE Threads::Threads)
endif()
