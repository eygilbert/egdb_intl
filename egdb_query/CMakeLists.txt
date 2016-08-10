file(GLOB_RECURSE source_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} 
    *.cpp 
    ../builddb/*.cpp 
    ../egdb/*.cpp 
    ../engine/*.cpp
)

file(RELATIVE_PATH project_relative_path ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp)
get_filename_component(t ${project_relative_path} PATH)

add_executable(${t}.main ${source_files})

target_compile_features(${t}.main PUBLIC 
    cxx_auto_type 
    cxx_lambdas
)

target_include_directories(${t}.main PUBLIC 
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(${t}.main 
    pthread
)
