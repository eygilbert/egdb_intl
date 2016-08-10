###############################################################
# Project settings
###############################################################

cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(egdbl_int CXX)

###############################################################
# COMPONENTS
###############################################################

add_subdirectory(egdb_query)
add_subdirectory(egdb_test)
add_subdirectory(example)
