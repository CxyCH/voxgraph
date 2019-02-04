//
// Created by victor on 29.01.19.
//

#ifndef VOXGRAPH_ODOMETRY_SIMULATOR_ODOMETRY_SIMULATOR_H_
#define VOXGRAPH_ODOMETRY_SIMULATOR_ODOMETRY_SIMULATOR_H_

#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/TransformStamped.h>
#include <kindr/minimal/rotation-quaternion.h>
#include <nav_msgs/Odometry.h>
#include <ros/ros.h>
#include <string>
#include "voxgraph/odometry_simulator/normal_distribution.h"

namespace voxgraph {
class OdometrySimulator {
 public:
  explicit OdometrySimulator(const ros::NodeHandle &nh,
                             const ros::NodeHandle &nh_private);
  ~OdometrySimulator() = default;

  void odometryCallback(const nav_msgs::Odometry::ConstPtr &odometry_msg);

 private:
  typedef kindr::minimal::RotationQuaternionTemplate<double> Rotation;
  typedef Eigen::Matrix<double, 3, 1> Vector3;

  // Internal pose = integrate(odom + noise on X_vel, Y_vel, Z_vel and Yaw_rate)
  geometry_msgs::PoseStamped internal_pose_;
  // Published pose = internal_pose_ + noise on X, Y, Z, Yaw, Pitch and Roll
  geometry_msgs::TransformStamped published_pose_;

  // ROS Node handles
  ros::NodeHandle nh_;
  ros::NodeHandle nh_private_;

  // Odometry subscriber and related settings
  ros::Subscriber odometry_subscriber_;
  int subscriber_queue_length_;
  std::string subscribe_to_odom_topic_;
  std::string publish_to_tf_frame_id_;

  // Noise distributions
  struct NoiseDistributions {
    NormalDistribution x, y, z;
    NormalDistribution yaw, pitch, roll;
    NormalDistribution x_vel, y_vel, z_vel;
    NormalDistribution yaw_rate;
  } noise_;

  // Transform publisher
  void publishCurrentPoseTf();
};
}  // namespace voxgraph

#endif  // VOXGRAPH_ODOMETRY_SIMULATOR_ODOMETRY_SIMULATOR_H_
