//
// Created by victor on 15.01.19.
//

#ifndef VOXGRAPH_BACKEND_CONSTRAINT_CONSTRAINT_H_
#define VOXGRAPH_BACKEND_CONSTRAINT_CONSTRAINT_H_

#include <ceres/ceres.h>
#include <memory>
#include "voxgraph/backend/node_collection.h"

namespace voxgraph {
class Constraint {
 public:
  typedef std::shared_ptr<Constraint> Ptr;
  typedef unsigned int ConstraintId;
  typedef Eigen::Matrix<double, 4, 4> InformationMatrix;

  struct Config {
    InformationMatrix information_matrix;
  };

  explicit Constraint(ConstraintId constraint_id, const Config &config);
  virtual ~Constraint() = default;

  virtual void addToProblem(const NodeCollection &node_collection,
                            ceres::Problem *problem) = 0;

  const ceres::ResidualBlockId getResidualBlockId() {
    return residual_block_id_;
  }

 protected:
  const ConstraintId constraint_id_;
  ceres::ResidualBlockId residual_block_id_ = nullptr;

  InformationMatrix sqrt_information_matrix_;
};
}  // namespace voxgraph

#endif  // VOXGRAPH_BACKEND_CONSTRAINT_CONSTRAINT_H_
