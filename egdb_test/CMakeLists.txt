###############################################################
# executables
###############################################################

file(GLOB_RECURSE source_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp ../builddb/*.cpp ../egdb/*.cpp ../engine/*.cpp)

add_executable(edgb_test.main ${source_files})
target_link_libraries(edgb_test.main pthread)
