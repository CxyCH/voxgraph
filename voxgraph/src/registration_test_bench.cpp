//
// Created by victor on 29.11.18.
//

#include <cblox/core/submap_collection.h>
#include <cblox/io/tsdf_submap_io.h>
#include <cblox/utils/quat_transformation_protobuf_utils.h>
#include <glog/logging.h>
#include <ros/ros.h>
#include <time.h>
#include <voxblox/io/layer_io.h>
#include <voxblox_ros/ptcloud_vis.h>
#include <boost/filesystem.hpp>
#include <string>
#include <vector>
#include "voxgraph/mapper/submap_collection/voxgraph_submap.h"
#include "voxgraph/pose_graph/constraint/submap_registration/submap_registerer.h"
#include "voxgraph/tools/tf_helper.h"
#include "voxgraph/tools/visualization/submap_visuals.h"

int main(int argc, char** argv) {
  enum SolverReportStyle { kBrief, kFull, kNone };

  // Start logging
  google::InitGoogleLogging(argv[0]);

  // Register with ROS master
  ros::init(argc, argv, "voxgraph_registration_test_bench");

  // Create node handles
  ros::NodeHandle nh;
  ros::NodeHandle nh_private("~");

  // Read ROS params: General experiment settings
  std::string submap_collection_file_path, log_folder_path;
  CHECK(nh_private.getParam("submap_collection_file_path",
                            submap_collection_file_path))
      << "Rosparam submap_collection_file_path must be set" << std::endl;
  CHECK(nh_private.getParam("log_folder_path", log_folder_path))
  << "Rosparam log_folder_path must be set" << std::endl;
  cblox::SubmapID reference_submap_id, reading_submap_id;
  {
    int fixed_submap_id_tmp, reading_submap_id_tmp;
    CHECK(nh_private.getParam("reference_submap_id", fixed_submap_id_tmp))
    << "Rosparam reference_submap_id must be set" << std::endl;
    CHECK(nh_private.getParam("reading_submap_id", reading_submap_id_tmp))
    << "Rosparam reading_submap_id must be set" << std::endl;
    reference_submap_id = static_cast<cblox::SubmapID>(fixed_submap_id_tmp);
    reading_submap_id = static_cast<cblox::SubmapID>(reading_submap_id_tmp);
  }
  std::vector<float> range_x, range_y, range_z;
  std::vector<float> range_yaw, range_pitch, range_roll;
  nh_private.param("test_range/x", range_x, {0});
  nh_private.param("test_range/y", range_y, {0});
  nh_private.param("test_range/z", range_z, {0});
  nh_private.param("test_range/yaw", range_yaw, {0});
  nh_private.param("test_range/pitch", range_pitch, {0});
  nh_private.param("test_range/roll", range_roll, {0});
  SolverReportStyle solver_report_style;
  {
    std::string report_style_str;
    nh_private.param<std::string>("solver_report_style", report_style_str,
                                  "brief");
    if (report_style_str == "brief") {
      solver_report_style = kBrief;
    } else if (report_style_str == "full") {
      solver_report_style = kFull;
    } else if (report_style_str == "none") {
      solver_report_style = kNone;
    } else {
      ROS_FATAL(
          "Param \"report_style\" must be "
          "\"brief\" (default), \"full\" or \"none\"");
      ros::shutdown();
      return -1;
    }
  }

  // Read ROS params: Submap registration settings
  ros::NodeHandle nh_registration(nh_private, "submap_registration");
  voxgraph::SubmapRegisterer::Options registerer_options;
  nh_registration.param("param/optimize_yaw",
                        registerer_options.param.optimize_yaw, true);
  nh_registration.param("solver/max_num_iterations",
                        registerer_options.solver.max_num_iterations, 40);
  nh_registration.param("solver/parameter_tolerance",
                        registerer_options.solver.parameter_tolerance, 3e-9);
  nh_registration.param("cost/min_voxel_weight",
                        registerer_options.cost.min_voxel_weight, 1e-6);
  nh_registration.param("cost/max_voxel_distance",
                        registerer_options.cost.max_voxel_distance, 0.6);
  nh_registration.param("cost/no_correspondence_cost",
                        registerer_options.cost.no_correspondence_cost, 0.0);
  nh_registration.param("cost/use_esdf_distance",
                        registerer_options.cost.use_esdf_distance, true);
  nh_registration.param("cost/visualize_residuals",
                        registerer_options.cost.visualize_residuals, false);
  nh_registration.param("cost/visualize_gradients",
                        registerer_options.cost.visualize_gradients, false);
  {
    std::string cost_function_type_str;
    nh_registration.param<std::string>("cost/cost_function_type",
                                       cost_function_type_str, "analytic");
    if (cost_function_type_str == "analytic") {
      registerer_options.cost.cost_function_type =
          voxgraph::SubmapRegisterer::Options::CostFunction::Type::kAnalytic;
    } else if (cost_function_type_str == "numeric") {
      registerer_options.cost.cost_function_type =
          voxgraph::SubmapRegisterer::Options::CostFunction::Type::kNumeric;
    } else {
      ROS_FATAL(
          "Param \"submap_registration/cost/cost_function_type\" "
          "must be \"analytic\" (default) or \"numeric\"");
      ros::shutdown();
      return -1;
    }
  }

  // Announce ROS topics for Rviz debug visuals
  ros::Publisher reference_mesh_pub =
      nh_private.advertise<visualization_msgs::Marker>("reference_mesh_pub", 1,
                                                       true);
  ros::Publisher reference_tsdf_pub =
      nh_private.advertise<pcl::PointCloud<pcl::PointXYZI>>(
          "reference_tsdf_pointcloud", 1, true);
  ros::Publisher perturbed_reading_mesh_pub =
      nh_private.advertise<visualization_msgs::Marker>(
          "perturbed_reading_mesh_pub", 1, true);
  ros::Publisher optimized_reading_mesh_pub =
      nh_private.advertise<visualization_msgs::Marker>(
          "optimized_reading_mesh_pub", 1, true);

  // Load the submap collection
  cblox::SubmapCollection<voxgraph::VoxgraphSubmap>::Ptr submap_collection_ptr;
  cblox::io::LoadSubmapCollection<voxgraph::VoxgraphSubmap>(
      submap_collection_file_path, &submap_collection_ptr);

  // If both submaps IDs are the same, duplicate the reference submap
  if (reference_submap_id == reading_submap_id) {
    ROS_INFO(
        "Reference and reading submap IDs are the same, "
        "duplicating the reference...");
    reading_submap_id = INT32_MAX;
    CHECK(submap_collection_ptr->duplicateSubMap(reference_submap_id,
                                                 reading_submap_id));
  }

  // Setup the submap to submap registerer
  voxgraph::SubmapRegisterer submap_registerer(submap_collection_ptr,
                                               registerer_options);

  // Setup visualization tools
  voxgraph::SubmapVisuals submap_vis(submap_collection_ptr->getConfig());

  // Save the reference submap pose and the original reading submap pose
  voxblox::Transformation transform_getter;
  CHECK(submap_collection_ptr->getSubMapPose(reference_submap_id,
                                             &transform_getter));
  const voxblox::Transformation T_world__reference = transform_getter;
  CHECK(submap_collection_ptr->getSubMapPose(reading_submap_id,
                                             &transform_getter));
  const voxblox::Transformation T_world__reading_original = transform_getter;
  const Eigen::Vector3f &ground_truth_position =
      T_world__reading_original.getPosition();

  // Publish TFs for the reference and reading submap
  voxgraph::TfHelper::publishTransform(T_world__reference, "world",
                                       "reference_submap", true);
  voxgraph::TfHelper::publishTransform(T_world__reading_original, "world",
                                       "reading_submap", true);

  // Publish the meshes used to visualize the submaps
  {
    // Wait for Rviz to launch so that it receives the meshes
    ros::Rate wait_rate(1);
    while (reference_mesh_pub.getNumSubscribers() == 0) {
      ROS_INFO(
          "Waiting for Rviz to launch "
          "and subscribe to topic 'reference_mesh_pub'");
      wait_rate.sleep();
    }
    // Publish reference submap Mesh
    submap_vis.publishMesh(*submap_collection_ptr, reference_submap_id,
                           voxblox::Color::Green(), "reference_submap",
                           reference_mesh_pub);
    // Publish temporary TFs for the moving meshes, such that Rviz
    // doesn't discard them due to missing frame position information
    voxgraph::TfHelper::publishTransform(T_world__reading_original, "world",
                                         "perturbed_submap", true);
    voxgraph::TfHelper::publishTransform(T_world__reading_original, "world",
                                         "optimized_submap", true);
    // Publish the reading submap mesh used to indicate its perturbed pose
    submap_vis.publishMesh(*submap_collection_ptr, reading_submap_id,
                           voxblox::Color::Red(), "perturbed_submap",
                           perturbed_reading_mesh_pub);
    // Publish the reading submap mesh used to indicate its optimized pose
    submap_vis.publishMesh(*submap_collection_ptr, reading_submap_id,
                           voxblox::Color::Blue(), "optimized_submap",
                           optimized_reading_mesh_pub);
  }

  // Generate the ESDFs for the submaps
  if (registerer_options.cost.use_esdf_distance) {
    CHECK(submap_collection_ptr->generateEsdfById(reference_submap_id));
    CHECK(submap_collection_ptr->generateEsdfById(reading_submap_id));
  }

  // Format log file path containing current time stamp
  time_t raw_time = std::time(nullptr);
  struct tm time_struct;
  localtime_r(&raw_time, &time_struct);
  char time_char_buffer[80];
  strftime(time_char_buffer, 80, "%Y-%m-%d_%H-%M-%S", &time_struct);
  boost::filesystem::path log_file_path;
  log_file_path = log_folder_path;
  log_file_path /= time_char_buffer;
  log_file_path += ".csv";
  ROS_INFO_STREAM("Log file will be saved as: " << log_file_path);

  // Create log file and write header
  // TODO(victorr): Write Git ID into log file
  // TODO(victorr): Check if log_folder_path exists and create it if it doesn't
  std::ofstream log_file;
  log_file.open(log_file_path.string());
  log_file << "reference_submap_id, reading_submap_id, "
           << "visuals_enabled, using_esdf_distance\n"
           << reference_submap_id << ",";
  if (reading_submap_id == INT32_MAX) {
    log_file << reference_submap_id;
  } else {
    log_file << reading_submap_id;
  }
  log_file << "," << (registerer_options.cost.visualize_residuals ||
                      registerer_options.cost.visualize_gradients)
           << "," << registerer_options.cost.use_esdf_distance << "\n"
           << "x_true, y_true, z_true, yaw_true, pitch_true, roll_true\n"
           << ground_truth_position.x() << "," << ground_truth_position.y()
           << "," << ground_truth_position.z() << ","
           << "0,0,0\n";
  log_file << "x_disturbance, y_disturbance, z_disturbance,"
           << "yaw_disturbance, pitch_disturbance, roll_disturbance, "
           << "x_error, y_error, z_error, yaw_error, pitch_error, roll_error, "
           << "solve_time \n";

  // Loop over all ranges
  int counter(0);
  ROS_INFO("Looping over all starting positions:");
  for (const float &x : range_x) {
    for (const float &y : range_y)
      for (const float &z : range_z)
        for (const float &yaw : range_yaw)
          for (const float &pitch : range_pitch)
            for (const float &roll : range_roll) {
              // Append test to log file
              log_file << x << "," << y << "," << z << "," << yaw << ","
                       << pitch << "," << roll << ",";

              // Move reading_submap to T(x,y,z,yaw,pitch,roll)
              // TODO(victorr): Implement disturbance over pitch & roll
              voxblox::Transformation::Vector3 rot_vec(0, 0, yaw);
              voxblox::Rotation rotation(rot_vec);
              voxblox::Transformation T_world__reading_perturbed;
              T_world__reading_perturbed.getRotation() =
                  T_world__reading_original.getRotation() * rotation;
              voxblox::Transformation::Vector3 translation(x, y, z);
              T_world__reading_perturbed.getPosition() =
                  T_world__reading_original.getPosition() + translation;
              submap_collection_ptr->setSubMapPose(reading_submap_id,
                                                   T_world__reading_perturbed);

              // Announce progress
              const Eigen::Vector3f &perturbed_position =
                  T_world__reading_perturbed.getPosition();
              printf(
                  "-- % 2i disturbance:        "
                  "x % 4.6f    y % 4.6f    z % 4.6f    "
                  "yaw % 4.2f    pitch % 4.2f    roll % 4.2f\n",
                  counter, perturbed_position.x() - ground_truth_position.x(),
                  perturbed_position.y() - ground_truth_position.y(),
                  perturbed_position.z() - ground_truth_position.z(),
                  yaw, pitch, roll);

              // Publish the TF of perturbed mesh
              voxgraph::TfHelper::publishTransform(T_world__reading_perturbed,
                                                   "world", "perturbed_submap",
                                                   true);

              // Set initial conditions
              ceres::Solver::Summary summary;
              voxblox::Transformation::Vector6 T_vec_read =
                  T_world__reading_perturbed.log();
              double world_pose_reading[4] = {T_vec_read[0], T_vec_read[1],
                                              T_vec_read[2], T_vec_read[5]};

              // Optimize submap registration
              bool registration_successful = submap_registerer.testRegistration(
                  reference_submap_id, reading_submap_id, world_pose_reading,
                  &summary);
              if (registration_successful) {
                // Update reading submap pose with the optimization result
                T_vec_read[0] = world_pose_reading[0];
                T_vec_read[1] = world_pose_reading[1];
                T_vec_read[2] = world_pose_reading[2];
                if (registerer_options.param.optimize_yaw) {
                  T_vec_read[5] = world_pose_reading[3];
                }
                const voxblox::Transformation T_world__reading_optimized =
                    voxblox::Transformation::exp(T_vec_read);
                submap_collection_ptr->setSubMapPose(
                    reading_submap_id, T_world__reading_optimized);

                // Announce results
                const Eigen::Vector3f &optimized_position =
                    T_world__reading_optimized.getPosition();
                printf(
                    "-- % 2i remaining error:    "
                    "x % 4.6f    y % 4.6f    z % 4.6f    "
                    "yaw % 4.2f    pitch % 4.2f    roll % 4.2f    "
                    "time % 4.4f\n",
                    counter++,
                    optimized_position.x() - ground_truth_position.x(),
                    optimized_position.y() - ground_truth_position.y(),
                    optimized_position.z() - ground_truth_position.z(),
                    T_world__reading_optimized.log()[5]
                        - T_world__reading_original.log()[5],
                    0.0,
                    0.0, summary.total_time_in_seconds);

                // Append stats to log file
                log_file << optimized_position.x() - ground_truth_position.x()
                         << ","
                         << optimized_position.y() - ground_truth_position.y()
                         << ","
                         << optimized_position.z() - ground_truth_position.z()
                         << ",0,0,0," << summary.total_time_in_seconds << "\n";
              } else {
                ROS_WARN("Ceres could not find a solution");

                // Append failure to log file
                log_file << "X,X,X,X,X,X\n";
              }

              // Report solver stats
              if (solver_report_style == kBrief) {
                std::cout << summary.BriefReport() << std::endl;
              } else if (solver_report_style == kFull) {
                std::cout << summary.FullReport() << std::endl;
              }

              // Exit if CTRL+C was pressed
              if (!ros::ok()) {
                ROS_INFO("Shutting down...");
                goto endloop;
              }
            }
  }
  endloop:

  // Close the log file and exit normally
  log_file.close();

  // Keep the ROS node alive in order to interact with its topics in Rviz
  ros::spin();

  return 0;
}
