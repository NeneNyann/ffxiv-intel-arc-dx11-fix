cmake_minimum_required(VERSION 4.2)

project(FFXIVIntelDX11Fix LANGUAGES CXX)

if(NOT WIN32)
   message(FATAL_ERROR "FFXIVIntelDX11Fix only supports Windows")
endif()
if(NOT MSVC)
   message(FATAL_ERROR "FFXIVIntelDX11Fix requires the MSVC toolchain")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
   message(FATAL_ERROR "FFXIVIntelDX11Fix must be built for x64")
endif()

get_filename_component(MSVC_TOOL_DIRECTORY "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(DUMPBIN_EXECUTABLE NAMES dumpbin
   HINTS "${MSVC_TOOL_DIRECTORY}" REQUIRED)
find_program(MSVC_LIB_EXECUTABLE NAMES lib
   HINTS "${MSVC_TOOL_DIRECTORY}" REQUIRED)

file(TO_CMAKE_PATH "$ENV{SystemRoot}/System32/winhttp.dll" SYSTEM_WINHTTP)
if(NOT EXISTS "${SYSTEM_WINHTTP}")
   message(FATAL_ERROR "System WinHTTP library not found: ${SYSTEM_WINHTTP}")
endif()

set(WINHTTP_EXPORT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/winhttp_exports")
set(WINHTTP_EXPORT_DEFINITION "${WINHTTP_EXPORT_DIRECTORY}/winhttp.def")
set(WINHTTP_EXPORT_LIBRARY "${WINHTTP_EXPORT_DIRECTORY}/forwarders.lib")
set(WINHTTP_EXPORT_OBJECT "${WINHTTP_EXPORT_DIRECTORY}/forwarders.exp")
set(WINHTTP_EXPORT_TOOL "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WinHTTPExports.cmake")

add_custom_command(
   OUTPUT "${WINHTTP_EXPORT_DEFINITION}" "${WINHTTP_EXPORT_LIBRARY}" "${WINHTTP_EXPORT_OBJECT}"
   COMMAND "${CMAKE_COMMAND}" -E make_directory "${WINHTTP_EXPORT_DIRECTORY}"
   COMMAND "${CMAKE_COMMAND}"
      "-DDUMPBIN=${DUMPBIN_EXECUTABLE}"
      "-DSYSTEM_WINHTTP=${SYSTEM_WINHTTP}"
      "-DOUTPUT_DEFINITION=${WINHTTP_EXPORT_DEFINITION}"
      -P "${WINHTTP_EXPORT_TOOL}"
   COMMAND "${MSVC_LIB_EXECUTABLE}" /nologo /machine:x64
      "/def:${WINHTTP_EXPORT_DEFINITION}" "/out:${WINHTTP_EXPORT_LIBRARY}"
   DEPENDS "${WINHTTP_EXPORT_TOOL}" "${SYSTEM_WINHTTP}"
   COMMENT "Generating the Windows WinHTTP export table"
   VERBATIM
)

set_source_files_properties("${WINHTTP_EXPORT_OBJECT}" PROPERTIES
   EXTERNAL_OBJECT TRUE
   GENERATED TRUE
)

add_library(winhttp SHARED
   src/runtime.h
   src/proxy.cpp
   src/driver_signatures.h
   src/driver_patch.cpp
   "${WINHTTP_EXPORT_OBJECT}"
)

target_compile_features(winhttp PRIVATE cxx_std_17)
target_compile_definitions(winhttp PRIVATE UNICODE _UNICODE)
target_compile_options(winhttp PRIVATE
   /W4 /WX /utf-8 /permissive-
   $<$<CONFIG:Release>:/Brepro>
   $<$<CONFIG:Release>:/experimental:deterministic>
   $<$<CONFIG:Release>:/pathmap:${CMAKE_CURRENT_SOURCE_DIR}=.>
)
target_link_libraries(winhttp PRIVATE Kernel32)
target_link_options(winhttp PRIVATE
   /INCREMENTAL:NO /OPT:REF /OPT:ICF
   $<$<CONFIG:Release>:/Brepro>
   $<$<CONFIG:Release>:/DEBUG:NONE>
)

set_target_properties(winhttp PROPERTIES
   PREFIX ""
   OUTPUT_NAME "winhttp"
   MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
   INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
   RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/bin"
   ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
   ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/lib"
   PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/pdb"
   PDB_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}/pdb"
)
