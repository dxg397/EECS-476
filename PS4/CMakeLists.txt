cmake_minimum_required(VERSION 2.8.3)
project(dxg397_PS4)

find_package(catkin_simple REQUIRED)


catkin_simple()

# example boost usage
# find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Boost REQUIRED COMPONENTS system thread)

# C++0x support - not quite the same as final C++11!
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

# Libraries
cs_add_library(ps4_traj_builder src/ps4_traj_builder.cpp)   

# Executables
cs_add_executable(ps4_lidar_alarm src/ps4_lidar_alarm.cpp)
cs_add_executable(ps4_pub_des_state src/ps4_pub_des_state_main.cpp src/ps4_pub_des_state.cpp)
cs_add_executable(ps4_open_loop_controller src/ps4_open_loop_controller.cpp)



#cs_add_executable(lidar_alarm2 src/lidar_alarm2.cpp)
# target_link_library(example my_lib)
target_link_libraries(ps4_pub_des_state ps4_traj_builder)

cs_install()
cs_export()
    
