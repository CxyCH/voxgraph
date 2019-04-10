//
// Created by victor on 18.01.19.
//

#ifndef VOXGRAPH_BACKEND_NODE_NODE_COLLECTION_H_
#define VOXGRAPH_BACKEND_NODE_NODE_COLLECTION_H_

#include <map>
#include <memory>
#include "voxgraph/backend/node/reference_frame_node.h"
#include "voxgraph/backend/node/submap_node.h"

namespace voxgraph {
class NodeCollection {
 public:
  typedef std::map<cblox::SubmapID, SubmapNode::Ptr> SubmapNodeMap;
  typedef std::map<Node::NodeId, ReferenceFrameNode::Ptr> ReferenceFrameNodeMap;

  NodeCollection();

  void addSubmapNode(const SubmapNode::Config &config);
  void addReferenceFrameNode(const ReferenceFrameNode::Config &config);

  ceres::LocalParameterization *getLocalParameterization() const {
    return local_parameterization_.get();
  }

  SubmapNode::Ptr getNodePtrBySubmapId(cblox::SubmapID submap_id) const;
  ReferenceFrameNode::Ptr getNodePtrByReferenceFrameId(
      ReferenceFrameNode::ReferenceFrameId) const;

  const SubmapNodeMap &getSubmapNodes() { return submap_nodes_; }

 private:
  Node::NodeId constraint_id_counter_ = 0;
  const Node::NodeId newConstraintId() { return constraint_id_counter_++; }

  SubmapNodeMap submap_nodes_;
  ReferenceFrameNodeMap reference_frame_nodes_;

  std::shared_ptr<ceres::LocalParameterization> local_parameterization_;
};
}  // namespace voxgraph

#endif  // VOXGRAPH_BACKEND_NODE_NODE_COLLECTION_H_
