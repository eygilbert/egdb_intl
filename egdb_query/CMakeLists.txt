###############################################################
# executables
###############################################################

file(GLOB_RECURSE source_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp ../builddb/*.cpp ../egdb/*.cpp ../engine/*.cpp)

add_executable(egdb_query.main ${source_files})
target_link_libraries(egdb_query.main pthread)
