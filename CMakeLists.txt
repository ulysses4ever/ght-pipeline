project(ele-pipeline)
cmake_minimum_required(VERSION 2.8)

find_package(Threads)

add_definitions(-std=c++11 -m64 -O2)
#add_definitions(-std=c++11 -m64 -g)


file(GLOB_RECURSE SRC_LIST "src/*.h" "src/*.cpp" "../notes.mkd" "../data/datasets.mkd" "../SourcererCC/clone-detector/sourcerer-cc.properties")

#aux_source_directory(. SRC_LIST)


include_directories(${CMAKE_SOURCE_DIR})




file(GLOB_RECURSE CLEANER_SRC "cleaner/*.h" "cleaner/*.cpp" "include/*.h" "include/*.cpp" "settings.csv" "hashes/*.cpp" "hashes/*.h")

#add_executable(cleaner ${CLEANER_SRC})
#target_link_libraries(cleaner ${CMAKE_THREAD_LIBS_INIT})

file(GLOB_RECURSE DOWNLOADER_SRC "downloader/*.h" "downloader/*.cpp" "include/*.h" "include/*.cpp" "settings.csv" "hashes/*.cpp" "hashes/*.h")

#add_executable(downloader ${DOWNLOADER_SRC})
#target_link_libraries(downloader ${CMAKE_THREAD_LIBS_INIT})

file(GLOB_RECURSE GHT_SRC "ght/*.h" "ght/*.cpp" "include/*.h" "include/*.cpp" "hashes/*.cpp" "hashes/*.h" "cleaner/*.h" "cleaner/*.cpp" "downloader/*.h" "downloader/*.cpp" "sovf_downloader/*.h" "sovf_downloader/*.cpp")

add_executable(ght ${GHT_SRC})
target_link_libraries(ght ${CMAKE_THREAD_LIBS_INIT})




