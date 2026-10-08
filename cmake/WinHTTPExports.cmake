execute_process(
   COMMAND "${DUMPBIN}" /nologo /exports "${SYSTEM_WINHTTP}"
   OUTPUT_VARIABLE DUMP_OUTPUT
   COMMAND_ERROR_IS_FATAL ANY
)

get_filename_component(SYSTEM_DIRECTORY "${SYSTEM_WINHTTP}" DIRECTORY)
file(TO_NATIVE_PATH "${SYSTEM_DIRECTORY}/winhttp" SYSTEM_WINHTTP_MODULE)

string(REPLACE "\n" ";" DUMP_LINES "${DUMP_OUTPUT}")
set(DEFINITION "LIBRARY winhttp\nEXPORTS\n")
set(EXPORT_COUNT 0)

foreach(LINE IN LISTS DUMP_LINES)
   if(LINE MATCHES
      "^[ \t]+([0-9]+)[ \t]+[0-9A-Fa-f]+[ \t]+([0-9A-Fa-f]+[ \t]+)?([^ \t=\r]+)")
      set(ORDINAL "${CMAKE_MATCH_1}")
      set(NAME "${CMAKE_MATCH_3}")
      if(NAME STREQUAL "[NONAME]")
         message(FATAL_ERROR "Unnamed WinHTTP export at ordinal ${ORDINAL}")
      endif()
      set(PRIVATE_EXPORT "")
      if(NAME STREQUAL "DllCanUnloadNow" OR NAME STREQUAL "DllGetClassObject")
         set(PRIVATE_EXPORT " PRIVATE")
      endif()
      string(APPEND DEFINITION "    ${NAME}=${SYSTEM_WINHTTP_MODULE}.${NAME} @${ORDINAL}${PRIVATE_EXPORT}\n")
      math(EXPR EXPORT_COUNT "${EXPORT_COUNT} + 1")
   endif()
endforeach()

if(EXPORT_COUNT EQUAL 0)
   message(FATAL_ERROR "No WinHTTP exports were found")
endif()

file(WRITE "${OUTPUT_DEFINITION}" "${DEFINITION}")
