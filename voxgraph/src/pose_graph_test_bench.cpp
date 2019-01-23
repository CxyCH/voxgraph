//
// Created by victor on 14.01.19.
//

#include "voxgraph/pose_graph/pose_graph.h"
#include <cblox/core/submap_collection.h>
#include <cblox/io/tsdf_submap_io.h>
#include <glog/logging.h>
#include <ros/ros.h>
#include <memory>
#include <string>
#include <vector>
#include "voxgraph/visualization/pose_graph_visuals.h"
#include "voxgraph/visualization/submap_visuals.h"
#include "voxgraph/voxgraph_submap.h"

int main(int argc, char** argv) {
  using voxgraph::VoxgraphSubmap;
  using voxgraph::PoseGraph;
  using voxgraph::SubmapNode;
  using voxgraph::RegistrationConstraint;
  using voxgraph::SubmapVisuals;

  // Start logging
  google::InitGoogleLogging(argv[0]);

  // Register with ROS master
  ros::init(argc, argv, "voxgraph_pose_graph_test_bench");

  // Create node handles
  ros::NodeHandle nh;
  ros::NodeHandle nh_private("~");

  // Read ROS params
  std::string submap_collection_file_path, log_folder_path;
  CHECK(nh_private.getParam("submap_collection_file_path",
                            submap_collection_file_path))
      << "Rosparam submap_collection_file_path must be set" << std::endl;
  CHECK(nh_private.getParam("log_folder_path", log_folder_path))
      << "Rosparam log_folder_path must be set" << std::endl;

  // Load the submap collection
  cblox::SubmapCollection<VoxgraphSubmap>::Ptr submap_collection_ptr;
  cblox::io::LoadSubmapCollection<VoxgraphSubmap>(submap_collection_file_path,
                                                  &submap_collection_ptr);

  // Create the pose graph
  PoseGraph pose_graph(submap_collection_ptr);

  // Add noise to the submap collection
  // TODO(victorr): This is a poor approximation of drift, since it doesn't
  //                distort the submaps themselves. Update the noisy odometer
  //                from voxgraph_mapper and use that instead.
  std::default_random_engine random_engine;
  std::normal_distribution<double> linear_noise_distrib(0.0, 1.0);
  std::vector<cblox::SubmapID> submap_ids = submap_collection_ptr->getIDs();
  for (const cblox::SubmapID& submap_id : submap_ids) {
    // NOTE: Submap 0 should not be perturbed,
    //       since its pose is not being optimized
    if (submap_id != 0) {
      // Get the submap pose; perturb it; then write it back to the collection
      voxblox::Transformation pose;
      CHECK(submap_collection_ptr->getSubMapPose(submap_id, &pose));
      pose.getPosition().x() += linear_noise_distrib(random_engine);
      pose.getPosition().y() += linear_noise_distrib(random_engine);
      pose.getPosition().z() += linear_noise_distrib(random_engine);
      submap_collection_ptr->setSubMapPose(submap_id, pose);
    }
  }

  // Generate the ESDFs for the submaps
  for (const cblox::SubmapID& submap_id : submap_ids) {
    CHECK(submap_collection_ptr->generateEsdfById(submap_id));
  }

  // Setup Rviz visualizations
  VoxgraphSubmap::Config submap_config;
  {
    // Configure the submap_config
    VoxgraphSubmap::ConstPtr submap_ptr =
        submap_collection_ptr->getSubMapConstPtrById(submap_ids[0]);
    CHECK_NOTNULL(submap_ptr);
    submap_config.tsdf_voxel_size =
        submap_ptr->getTsdfMap().getTsdfLayer().voxel_size();
    submap_config.tsdf_voxels_per_side =
        size_t(submap_ptr->getTsdfMap().getTsdfLayer().block_size() /
               submap_config.tsdf_voxel_size);
  }
  SubmapVisuals submap_vis(submap_config);
  ros::Publisher separated_mesh_original_pub =
      nh_private.advertise<visualization_msgs::Marker>(
          "separated_mesh_original", 1, true);
  ros::Publisher separated_mesh_optimized_pub =
      nh_private.advertise<visualization_msgs::Marker>(
          "separated_mesh_optimized", 1, true);
  ros::Publisher bounding_boxes_pub =
      nh_private.advertise<visualization_msgs::Marker>("bounding_boxes", 100,
                                                       true);
  ros::Publisher pose_graph_edge_original_pub =
      nh_private.advertise<visualization_msgs::Marker>(
          "pose_graph_original_edges", 100, true);
  ros::Publisher pose_graph_edge_optimized_pub =
      nh_private.advertise<visualization_msgs::Marker>(
          "pose_graph_optimized_edges", 100, true);

  // Show the original submap meshes in Rviz
  submap_vis.publishSeparatedMesh(*submap_collection_ptr, "world",
                                  separated_mesh_original_pub);

  // Add all submaps as nodes
  std::cout << "Adding all submaps as nodes" << std::endl;
  for (const cblox::SubmapID& submap_id : submap_ids) {
    SubmapNode::Config node_config;
    node_config.submap_id = submap_id;
    CHECK(submap_collection_ptr->getSubMapPose(
        submap_id, &node_config.initial_submap_pose));
    if (submap_id == 0) {
      std::cout << "Setting pose of submap 0 to constant" << std::endl;
      node_config.set_constant = true;
    } else {
      node_config.set_constant = false;
    }
    pose_graph.addNode(node_config);
  }

  // Add odometry constraints between the submaps
  for (const cblox::SubmapID& submap_id : submap_ids) {
    // TODO(victorr): Implement odometry constraints
  }

  // Add a registration constraint for each overlapping submap pair
  std::cout << "Adding registration constraint from submap " << std::endl;
  for (unsigned int i = 0; i < submap_ids.size(); i++) {
    // Get a pointer to the first submap
    cblox::SubmapID first_submap_id = submap_ids[i];
    VoxgraphSubmap::ConstPtr first_submap_ptr =
        submap_collection_ptr->getSubMapConstPtrById(first_submap_id);
    CHECK_NOTNULL(first_submap_ptr);

    // Publish the submap's bounding boxes
    submap_vis.publishBox(first_submap_ptr->getWorldFrameSurfaceObbCorners(),
                          voxblox::Color::Blue(), "world",
                          "surface_obb" + std::to_string(first_submap_id),
                          bounding_boxes_pub);
    submap_vis.publishBox(first_submap_ptr->getWorldFrameSurfaceAabbCorners(),
                          voxblox::Color::Red(), "world",
                          "surface_aabb" + std::to_string(first_submap_id),
                          bounding_boxes_pub);
    submap_vis.publishBox(first_submap_ptr->getWorldFrameSubmapObbCorners(),
                          voxblox::Color::Blue(), "world",
                          "submap_obb" + std::to_string(first_submap_id),
                          bounding_boxes_pub);
    submap_vis.publishBox(first_submap_ptr->getWorldFrameSubmapAabbCorners(),
                          voxblox::Color::Red(), "world",
                          "submap_aabb" + std::to_string(first_submap_id),
                          bounding_boxes_pub);

    for (unsigned int j = i + 1; j < submap_ids.size(); j++) {
      // Get a pointer to the second submap
      cblox::SubmapID second_submap_id = submap_ids[j];
      VoxgraphSubmap::ConstPtr second_submap_ptr =
          submap_collection_ptr->getSubMapConstPtrById(second_submap_id);
      CHECK_NOTNULL(second_submap_ptr);

      // Check whether the first and second submap overlap
      if (first_submap_ptr->overlapsWith(second_submap_ptr)) {
        // Add the constraint
        RegistrationConstraint::Config constraint_config = {first_submap_id,
                                                            second_submap_id};
        pose_graph.addConstraint(constraint_config);
      }
    }
  }

  // Publish the unoptimized pose graph
  voxgraph::PoseGraphVisuals pose_graph_vis;
  pose_graph.initialize();
  pose_graph_vis.publishPoseGraph(pose_graph, "world", "edges",
                                  pose_graph_edge_original_pub);

  // Optimize the graph
  std::cout << "Optimizing the graph" << std::endl;
  pose_graph.optimize();

  // Update the submap poses
  for (const auto& submap_pose_kv : pose_graph.getSubmapPoses()) {
    submap_collection_ptr->setSubMapPose(submap_pose_kv.first,
                                         submap_pose_kv.second);
  }

  // Show the optimized submap meshes in Rviz
  submap_vis.publishSeparatedMesh(*submap_collection_ptr, "world",
                                  separated_mesh_optimized_pub);

  // Publish the optimized pose graph
  pose_graph_vis.publishPoseGraph(pose_graph, "world", "edges",
                                  pose_graph_edge_optimized_pub);

  // Keep the ROS node alive in order to interact with its topics in Rviz
  std::cout << "Done" << std::endl;
  ros::spin();

  return 0;
}
