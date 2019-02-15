#include <ros/ros.h> //Must include this for all ROS cpp projects
#include <sensor_msgs/LaserScan.h>
#include <std_msgs/Float32.h> //Including the Float32 class from std_msgs
#include <std_msgs/Bool.h> // boolean message 


const double MIN_SAFE_DISTANCE = 1.0; // set alarm if anything is within 0.5m of the front of robot

// these values to be set within the laser callback
float ping_dist_in_front_=3.0; // global var to hold length of a SINGLE LIDAR ping--in front
int ping_index_= -1; // NOT real; callback will have to find this
double angle_min_=0.0;
double angle_max_=0.0;
double angle_increment_=0.0;
double range_min_ = 0.0;
double range_max_ = 0.0;
bool laser_alarm_=false;

ros::Publisher lidar_alarm_publisher_;
ros::Publisher lidar_dist_publisher_;
// really, do NOT want to depend on a single ping.  Should consider a subset of pings
// to improve reliability and avoid false alarms or failure to see an obstacle

void laserCallback(const sensor_msgs::LaserScan& laser_scan) {
    if (ping_index_<0)  {
        //for first message received, set up the desired index of LIDAR range to eval
        angle_min_ = laser_scan.angle_min;
        angle_max_ = laser_scan.angle_max;
        angle_increment_ = laser_scan.angle_increment;
        range_min_ = laser_scan.range_min;
        range_max_ = laser_scan.range_max;
        // what is the index of the ping that is straight ahead?
        // BETTER would be to use transforms, which would reference how the LIDAR is mounted;
        // but this will do for simple illustration
        ping_index_ = (int) ((0.0 -angle_min_)/angle_increment_);
        ROS_INFO("LIDAR setup: ping_index = %d",ping_index_);
        
    }
    
   // Constants for how many pings on either side to check (in addition to the middle one)
    int left_offset = -10;
    int right_offset = 10;
    // Boolean holding variable to OR the results from each ping check
    bool unsafe_ping = false;

    ROS_INFO("STARTING PING RANGE TESTING");


    // My main for loop, which checks all these pings, one by one
    for (int ping_index_offset = left_offset; ping_index_offset < right_offset; ping_index_offset++)
    {
        // Increase index for this loop
        ping_index_ += ping_index_offset;

        // Extract distance
        ping_dist_in_front_ = laser_scan.ranges[ping_index_];
        ROS_INFO("ping dist in front = %f",ping_dist_in_front_);

        // Check distance
        if (ping_dist_in_front_<MIN_SAFE_DISTANCE)
        {
            // Make the holding variable true, if any of the pings were too close
            if (unsafe_ping == false)
            {
                unsafe_ping = true;
            }
            else
            {
                // Extraneous, but w/e
                ROS_INFO("A DUPLICATE UNSAFE PING WAS FOUND");
            }
        }
    }

    // Same checking as before
    if (unsafe_ping == true) {
        ROS_WARN("DANGER, WILL Devansh!!");
        laser_alarm_=true;
    }
    else {
        laser_alarm_=false;
    }

    // Technically unnecessary
    unsafe_ping == false;

   std_msgs::Bool lidar_alarm_msg;
   lidar_alarm_msg.data = laser_alarm_;
   lidar_alarm_publisher_.publish(lidar_alarm_msg);
   std_msgs::Float32 lidar_dist_msg;
   lidar_dist_msg.data = ping_dist_in_front_;
   lidar_dist_publisher_.publish(lidar_dist_msg);   
}

int main(int argc, char **argv) {
    ros::init(argc, argv, "lidar_alarm"); //name this node
    ros::NodeHandle nh; 
    //create a Subscriber object and have it subscribe to the lidar topic
    ros::Publisher pub = nh.advertise<std_msgs::Bool>("lidar_alarm", 1);
    lidar_alarm_publisher_ = pub; // let's make this global, so callback can use it
    ros::Publisher pub2 = nh.advertise<std_msgs::Float32>("lidar_dist", 1);  
    lidar_dist_publisher_ = pub2;
    ros::Subscriber lidar_subscriber = nh.subscribe("scan", 1, laserCallback);
    ros::spin(); //this is essentially a "while(1)" statement, except it
    // forces refreshing wakeups upon new data arrival
    // main program essentially hangs here, but it must stay alive to keep the callback function alive
    return 0; // should never get here, unless roscore dies
}

