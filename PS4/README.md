## Running the program

Launch gazebo:
roslaunch mobot_urdf mobot_in_pen.launch

Run lidar_alarm code:
rosrun dxg397_PS4 ps4_lidar_alarm

Run open_loop_controller:
rosrun dxg397_PS4 ps4_open_loop_controller 

Run the publisher:
rosrun dxg397_PS4 ps4_pub_des_states 

To call e-stop:
rosservice call estop_service

To reset e-stop:
rosservice call clear_estop_service
