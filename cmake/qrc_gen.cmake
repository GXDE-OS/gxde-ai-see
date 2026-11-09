# generates QRC file for frontend web UI.

if (NOT DIR OR NOT OUTPUT)
    message(FATAL_ERROR "DIR and OUTPUT are required")
endif ()

get_filename_component(dir_name "${DIR}" NAME)
file(GLOB_RECURSE files RELATIVE "${DIR}" "${DIR}/*")
list(SORT files)
if (NOT files)
    message(FATAL_ERROR "No files found in ${DIR}")
endif ()

set(content "<!DOCTYPE RCC>\n<RCC version=\"1.0\">\n<qresource prefix=\"/\">\n")
foreach(file IN LISTS files)
    string(APPEND content "    <file>${dir_name}/${file}</file>\n")
endforeach()
string(APPEND content "</qresource>\n</RCC>\n")

file(WRITE "${OUTPUT}" "${content}")
