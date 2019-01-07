//
// Created by victor on 15.12.18.
//

#ifndef VOXGRAPH_SUBMAP_REGISTRATION_REGISTRATION_COST_FUNCTION_H_
#define VOXGRAPH_SUBMAP_REGISTRATION_REGISTRATION_COST_FUNCTION_H_

#include <ceres/ceres.h>
#include <voxblox/interpolator/interpolator.h>
#include <cblox/core/common.h>
#include <cblox/core/submap_collection.h>
#include <cblox/core/tsdf_submap.h>
#include <cblox/core/tsdf_esdf_submap.h>
#include "voxgraph/submap_registration/submap_registerer.h"

// For visualization only
#include <ros/ros.h>
#include "voxgraph/visualization/cost_function_visuals.h"

namespace voxgraph {
class RegistrationCostFunction : public ceres::CostFunction {
 public:
  RegistrationCostFunction(cblox::TsdfSubmap::ConstPtr reference_submap_ptr,
                           cblox::TsdfSubmap::ConstPtr reading_submap_ptr,
                           SubmapRegisterer::Options::CostFunction options);

  bool getVoxelsAndQVector(const voxblox::Layer<voxblox::TsdfVoxel> &layer,
                           const voxblox::Point &pos,
                           const voxblox::TsdfVoxel **neighboring_voxels,
                           voxblox::InterpVector *q_vector) const;

  bool Evaluate(double const *const *parameters, double *residuals,
                double **jacobians) const override;

  const unsigned int getNumRelevantVoxels() const {
    return num_relevant_reference_voxels_;
  }

 private:
  // Cost function options
  SubmapRegisterer::Options::CostFunction options_;

  // Pointers and const refs to the submaps that will be aligned
  cblox::TsdfSubmap::ConstPtr ref_submap_ptr_;
  cblox::TsdfSubmap::ConstPtr reading_submap_ptr_;
  const voxblox::Layer<voxblox::TsdfVoxel> &reference_layer_;
  const voxblox::Layer<voxblox::TsdfVoxel> &reading_layer_;

  // Block and voxel index hash map storing
  // only the relevant voxels (observed and within truncation distance)
  voxblox::HierarchicalIndexMap reference_block_voxel_list_;
  unsigned int num_relevant_reference_voxels_ = 0;

  // Used for residual and Jacobian visualization
  mutable CostFunctionVisuals cost_function_visuals_;

  // This matrix is used to interpolate voxels
  // It corresponds to matrix B_1 from paper: http://spie.org/samples/PM159.pdf
  // clang-format off
  const voxblox::InterpTable interp_table_ =
      (voxblox::InterpTable() <<
        1,  0,  0,  0,  0,  0,  0,  0,
       -1,  0,  0,  0,  1,  0,  0,  0,
       -1,  0,  1,  0,  0,  0,  0,  0,
       -1,  1,  0,  0,  0,  0,  0,  0,
        1,  0, -1,  0, -1,  0,  1,  0,
        1, -1, -1,  1,  0,  0,  0,  0,
        1, -1,  0,  0, -1,  1,  0,  0,
       -1,  1,  1, -1,  1, -1, -1,  1).finished();
  // clang-format on
};
}  // namespace voxgraph

#endif  // VOXGRAPH_SUBMAP_REGISTRATION_REGISTRATION_COST_FUNCTION_H_
