//
// Created by victor on 17.01.19.
//

#include "voxgraph/backend/node/submap_node.h"
#include <utility>

namespace voxgraph {
SubmapNode::SubmapNode(NodeId node_id, SubmapNode::Config config)
    : Node(node_id), config_(std::move(config)) {
  // Set the node's pose to the initial submap pose
  voxblox::Transformation::Vector6 T_vec = config.initial_submap_pose.log();
  world_node_pose_[0] = T_vec[0];
  world_node_pose_[1] = T_vec[1];
  world_node_pose_[2] = T_vec[2];
  world_node_pose_[3] = T_vec[5];

  // Indicate whether the pose should be optimized or kept constant
  constant_ = config_.set_constant;
}

const voxblox::Transformation SubmapNode::getSubmapPose() const {
  voxblox::Transformation::Vector6 T_vec = config_.initial_submap_pose.log();
  T_vec[0] = world_node_pose_[0];
  T_vec[1] = world_node_pose_[1];
  T_vec[2] = world_node_pose_[2];
  T_vec[5] = world_node_pose_[3];
  return voxblox::Transformation::exp(T_vec);
}
}  // namespace voxgraph
