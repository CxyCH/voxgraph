//
// Created by victor on 04.12.18.
//

#ifndef VOXGRAPH_SUBMAP_REGISTRATION_SUBMAP_REGISTERER_H_
#define VOXGRAPH_SUBMAP_REGISTRATION_SUBMAP_REGISTERER_H_

#include <cblox/core/common.h>
#include <cblox/core/submap_collection.h>
#include <cblox/core/tsdf_esdf_submap.h>
#include <cblox/core/tsdf_submap.h>
#include <ceres/ceres.h>
#include "voxgraph/voxgraph_submap.h"

namespace voxgraph {
class SubmapRegisterer {
 public:
  struct Options {
    ceres::Solver::Options solver;
    struct CostFunction {
      enum Type { kAnalytic = 0, kNumeric } cost_function_type;
      double min_voxel_weight = 1e-6;
      double max_voxel_distance = 0.3;
      // Cost to assign for voxels that can't be interpolated
      double no_correspondence_cost = 0;
      bool use_esdf_distance = true;
      bool visualize_residuals = false;
      bool visualize_gradients = false;
    } cost;
  };

  SubmapRegisterer(
      cblox::SubmapCollection<VoxgraphSubmap>::ConstPtr submap_collection_ptr,
      const Options &options);

  ~SubmapRegisterer() = default;

  bool testRegistration(const cblox::SubmapID &reference_submap_id,
                        const cblox::SubmapID &reading_submap_id,
                        double *world_t_world_reading,
                        ceres::Solver::Summary *summary);

 private:
  cblox::SubmapCollection<VoxgraphSubmap>::ConstPtr submap_collection_ptr_;
  Options options_;
};
}  // namespace voxgraph

#endif  // VOXGRAPH_SUBMAP_REGISTRATION_SUBMAP_REGISTERER_H_
