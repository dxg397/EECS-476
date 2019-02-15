#include "ps4_pub_des_state.h"
 
//ExampleRosClass::ExampleRosClass(ros::NodeHandle* nodehandle):nh_(*nodehandle)

PS4DesStatePublisher::PS4DesStatePublisher(ros::NodeHandle& nh) : nh_(nh) {
    //as_(nh, "pub_des_state_server", boost::bind(&PS4DesStatePublisher::executeCB, this, _1),false) {
    //as_.start(); //start the server running
    //configure the trajectory builder: 
    //dt_ = dt; //send desired-state messages at fixed rate, e.g. 0.02 sec = 50Hz
    trajBuilder_.set_dt(dt);
    //dynamic parameters: should be tuned for target system
    accel_max_ = accel_max;
    trajBuilder_.set_accel_max(accel_max_);
    alpha_max_ = alpha_max;
    trajBuilder_.set_alpha_max(alpha_max_);
    speed_max_ = speed_max;
    trajBuilder_.set_speed_max(speed_max_);
    omega_max_ = omega_max;
    trajBuilder_.set_omega_max(omega_max_);
    path_move_tol_ = path_move_tol;
    trajBuilder_.set_path_move_tol_(path_move_tol_);
    initializePublishers();
    initializeServices();
    initializeSubscribers();

    //define a halt state; zero speed and spin, and fill with viable coords
    halt_twist_.linear.x = 0.0;
    halt_twist_.linear.y = 0.0;
    halt_twist_.linear.z = 0.0;
    halt_twist_.angular.x = 0.0;
    halt_twist_.angular.y = 0.0;
    halt_twist_.angular.z = 0.0;
    motion_mode_ = DONE_W_SUBGOAL; //init in state ready to process new goal
    e_stop_trigger_ = false; //these are intended to enable e-stop via a service
    e_stop_reset_ = false; //and reset estop
    g_lidar_alarm = false;
    e_stop_alarm = false; // Might be redundant, modified for lab 6
    on_alarm = false;
    e_stop_on = false; // Might be redundant, modified for lab 6
    current_pose_ = trajBuilder_.xyPsi2PoseStamped(0,0,0);
    start_pose_ = current_pose_;
    end_pose_ = current_pose_;
    current_des_state_.twist.twist = halt_twist_;
    current_des_state_.pose.pose = current_pose_.pose;
    halt_state_ = current_des_state_;
    seg_start_state_ = current_des_state_;
    seg_end_state_ = current_des_state_;
}

void PS4DesStatePublisher::initializeServices() {
    ROS_INFO("Initializing Services");
    estop_service_ = nh_.advertiseService("estop_service",
            &PS4DesStatePublisher::estopServiceCallback, this);
    estop_clear_service_ = nh_.advertiseService("clear_estop_service",
            &PS4DesStatePublisher::clearEstopServiceCallback, this);
    flush_path_queue_ = nh_.advertiseService("flush_path_queue_service",
            &PS4DesStatePublisher::flushPathQueueCB, this);
    append_path_ = nh_.advertiseService("append_path_queue_service",
            &PS4DesStatePublisher::appendPathQueueCB, this);
    
}

void PS4DesStatePublisher::initializeSubscribers() {
    alarm_subscriber_ = nh_.subscribe("/lidar_alarm", 1, &PS4DesStatePublisher::alarmCB, this); // Modified by Jonathan
    //dist_subscriber_ = nh_.subscribe("lidar_dist", 1, &PS4DesStatePublisher::alarmCB, this); // Modified by Jonathan
    ESTOP_ = nh_.subscribe("/ESTOP", 1, &PS4DesStatePublisher::estopCB, this); // Modified by Jonathan
}

//member helper function to set up publishers;

void PS4DesStatePublisher::initializePublishers() {
    ROS_INFO("Initializing Publishers");
    desired_state_publisher_ = nh_.advertise<nav_msgs::Odometry>("/desState", 1, true);
    des_psi_publisher_ = nh_.advertise<std_msgs::Float64>("/desPsi", 1);
}

// Following code added by Jonathan

void PS4DesStatePublisher::alarmCB(const std_msgs::Bool& alarm_msg) 
{ 
  g_lidar_alarm = alarm_msg.data; 
  if (g_lidar_alarm) {
     ROS_WARN("lidar stop!!"); 
	if (!on_alarm) {
		on_alarm = true;
                e_stop_trigger_ = true;
                       }
                     }
  else {
	if (on_alarm) {
		ROS_INFO("lidar stop reset");
		on_alarm = false;
		e_stop_reset_ = true;
                      }
       }
}

// Code modified by Jonathan stopped

// Code modified for lab 6

void PS4DesStatePublisher::estopCB(const std_msgs::Bool& estop_msg) 
{ 
  e_stop_alarm = estop_msg.data;
  if (e_stop_alarm) {
     ROS_WARN("ESTOP ACTIVATED");
	if (!e_stop_on) {
		e_stop_on = true;
                e_stop_trigger_ = true;
                       }
                     }
  else {
	if (e_stop_on) {
		ROS_INFO("estop reset");
		e_stop_on = false;
		e_stop_reset_ = true;
                      }
       }
 
}

bool PS4DesStatePublisher::estopServiceCallback(std_srvs::TriggerRequest& request, std_srvs::TriggerResponse& response) {
ROS_INFO("Estop trigger service callback");
	on_alarm = true; // 
	e_stop_trigger_ = true; // 
}

bool PS4DesStatePublisher::clearEstopServiceCallback(std_srvs::TriggerRequest& request, std_srvs::TriggerResponse& response) {
ROS_INFO("Estop reset service callback");
   on_alarm = false; 
  e_stop_reset_ = true; 
}

// Code modified for lab 6 stopped

bool PS4DesStatePublisher::flushPathQueueCB(std_srvs::TriggerRequest& request, std_srvs::TriggerResponse& response) {
    ROS_WARN("flushing path queue");
    while (!path_queue_.empty())
    {
        path_queue_.pop();
    }
    return true;
}

bool PS4DesStatePublisher::appendPathQueueCB(dxg397_PS4::pathRequest& request, dxg397_PS4::pathResponse& response) {

    int npts = request.path.poses.size();
    ROS_INFO("appending path queue with %d points", npts);
    for (int i = 0; i < npts; i++) {
        path_queue_.push(request.path.poses[i]);
    }
    return true;
}

void PS4DesStatePublisher::set_init_pose(double x, double y, double psi) {
    current_pose_ = trajBuilder_.xyPsi2PoseStamped(x, y, psi);
}

//here is a state machine to advance desired-state publications
// this will cause a single desired state to be published
// The state machine advances through modes, including
// HALTING, E_STOPPED, DONE_W_SUBGOAL, and PURSUING_SUBGOAL
// does PURSUING_SUBGOAL->DONE_W_SUBGOAL->PURSUING_SUBGOAL
// or HALTING->E_STOPPED->DONE_W_SUBGOAL->PURSUING_SUBGOAL
// transition to HALTING requires triggering an e-stop via service _service_
// transition from HALTING to E_STOPPED occurs with completing of halt trajectory
// transition from E_STOPPED to DONE_W_SUBGOAL requires estop reset via 
//   service estop_clear_service_
// transition DONE_W_SUBGOAL->PURSUING_SUBGOAL depends on at least one path
//   point in the queue path_queue_
// transition PURSUING_SUBGOAL->DONE_W_SUBGOAL occurs when current subgoal has
//   been reached
// path queue can be flushed via service flush_path_queue_,
// or points can be appended to path queue w/ service append_path_

void PS4DesStatePublisher::pub_next_state() {
    // first test if an e-stop has been triggered
    if (e_stop_trigger_) { 
		ROS_WARN("in estop mode before trajbuilder");
        e_stop_trigger_ = false; //reset trigger
        //compute a halt trajectory
        trajBuilder_.build_braking_traj(current_pose_, des_state_vec_);
        motion_mode_ = HALTING;
        traj_pt_i_ = 0;
        npts_traj_ = des_state_vec_.size();
    }
    //or if an e-stop has been cleared
    if (e_stop_reset_) {
        e_stop_reset_ = false; //reset trigger
        if (motion_mode_ != E_STOPPED) {
            ROS_WARN("e-stop reset while not in e-stop mode");
        }
        //OK...want to resume motion from e-stopped mode;
        else {
            motion_mode_ = DONE_W_SUBGOAL; //this will pick up where left off
        }
    }
    
    //state machine; results in publishing a new desired state
    switch (motion_mode_) {
        case E_STOPPED: //this state must be reset by a service
            desired_state_publisher_.publish(halt_state_);
            break;

        case HALTING: //e-stop service callback sets this mode
            //if need to brake from e-stop, service will have computed
            // new des_state_vec_, set indices and set motion mode;
            current_des_state_ = des_state_vec_[traj_pt_i_];
            current_des_state_.header.stamp = ros::Time::now();
            desired_state_publisher_.publish(current_des_state_);
            current_pose_.pose = current_des_state_.pose.pose;
            current_pose_.header = current_des_state_.header;
            des_psi_ = trajBuilder_.convertPlanarQuat2Psi(current_pose_.pose.orientation);
            float_msg_.data = des_psi_;
            des_psi_publisher_.publish(float_msg_); 
            
            traj_pt_i_++;
            //segue from braking to halted e-stop state;
            if (traj_pt_i_ >= npts_traj_) { //here if completed all pts of braking traj
                halt_state_ = des_state_vec_.back(); //last point of halting traj
                // make sure it has 0 twist
                halt_state_.twist.twist = halt_twist_;
                seg_end_state_ = halt_state_;
                current_des_state_ = seg_end_state_;
                motion_mode_ = E_STOPPED; //change state to remain halted                    
            }
			else {
			     motion_mode_ = E_STOPPED; //change state to remain halted
			     }  
            break;

        case PURSUING_SUBGOAL: //if have remaining pts in computed traj, send them
            //extract the i'th point of our plan:
            current_des_state_ = des_state_vec_[traj_pt_i_];
            current_pose_.pose = current_des_state_.pose.pose;
            current_des_state_.header.stamp = ros::Time::now();
            desired_state_publisher_.publish(current_des_state_);
            //next three lines just for convenience--convert to heading and publish
            // for rqt_plot visualization            
            des_psi_ = trajBuilder_.convertPlanarQuat2Psi(current_pose_.pose.orientation);
            float_msg_.data = des_psi_;
            des_psi_publisher_.publish(float_msg_); 
            traj_pt_i_++; // increment counter to prep for next point of plan
            //check if we have clocked out all of our planned states:
            if (traj_pt_i_ >= npts_traj_) {
                motion_mode_ = DONE_W_SUBGOAL; //if so, indicate we are done
                seg_end_state_ = des_state_vec_.back(); // last state of traj
                if (!path_queue_.empty()) { 
                    path_queue_.pop(); // done w/ this subgoal; remove from the queue 
                }
                ROS_INFO("reached a subgoal: x = %f, y= %f",current_pose_.pose.position.x,
                        current_pose_.pose.position.y);
            }
            break;

        case DONE_W_SUBGOAL: //suspended, pending a new subgoal
            //see if there is another subgoal is in queue; if so, use
            //it to compute a new trajectory and change motion mode

            if (!path_queue_.empty()) {
                int n_path_pts = path_queue_.size();
                ROS_INFO("%d points in path queue",n_path_pts);
                start_pose_ = current_pose_;
                end_pose_ = path_queue_.front();
                trajBuilder_.build_point_and_go_traj(start_pose_, end_pose_,des_state_vec_);
                traj_pt_i_ = 0;
                npts_traj_ = des_state_vec_.size();
                motion_mode_ = PURSUING_SUBGOAL; // got a new plan; change mode to pursue it
                ROS_INFO("computed new trajectory to pursue");
            } else { //no new goal? stay halted in this mode 
                // by simply reiterating the last state sent (should have zero vel)
                desired_state_publisher_.publish(seg_end_state_);
            }
            break;

        default: //this should not happen
            ROS_WARN("motion mode not recognized!");
            desired_state_publisher_.publish(current_des_state_);
            break;
    }
}
