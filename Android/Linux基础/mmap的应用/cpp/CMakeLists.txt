

cmake_minimum_required(VERSION 3.10.2)



project("logger")

aux_source_directory(. DIR_LIB_SRCS)
ADD_LIBRARY(native-lib SHARED ${DIR_LIB_SRCS})



find_library(log-lib log)

target_link_libraries(native-lib ${log-lib} )