cmake_minimum_required(VERSION 3.16)
project(kv_cpp VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(kv_store
  src/kv_store.cpp
  src/wal.cpp
  src/raft.cpp
  src/raft_transport.cpp
)
target_include_directories(kv_store PUBLIC include)

add_executable(kv_cli
  src/main.cpp
)
target_link_libraries(kv_cli PRIVATE kv_store)

add_executable(raft_demo
  src/raft_demo.cpp
)
target_link_libraries(raft_demo PRIVATE kv_store)

add_executable(kv_server
  src/server_main.cpp
)

target_link_libraries(kv_server PRIVATE kv_store)

add_executable(kv_cluster_demo
  src/cluster_demo.cpp
)

target_link_libraries(kv_cluster_demo PRIVATE kv_store)


add_executable(kv_client
  src/kv_client.cpp
)