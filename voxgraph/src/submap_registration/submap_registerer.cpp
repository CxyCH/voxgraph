//
// Created by victor on 04.12.18.
//

#include <voxblox/interpolator/interpolator.h>
#include "voxgraph/submap_registration/submap_registerer.h"
#include "voxgraph/submap_registration/registration_cost_function.h"

namespace voxgraph {
  SubmapRegisterer::SubmapRegisterer(cblox::TsdfSubmapCollection::ConstPtr tsdf_submap_collection_ptr,
                                     Options& options) :
                                     tsdf_submap_collection_ptr_(std::move(tsdf_submap_collection_ptr)),
                                     options_(options) {}

  bool SubmapRegisterer::findRegistration(const cblox::SubmapID &reference_submap_id,
                                          const cblox::SubmapID &reading_submap_id,
                                          double *ref_t_ref_reading,
                                          ceres::Solver::Summary &summary) {
    // Get shared pointers to the reference and reading submaps
    cblox::TsdfSubmap::ConstPtr reference_submap_ptr, reading_submap_ptr;
    CHECK(tsdf_submap_collection_ptr_->getTsdfSubmapConstPtrById(reference_submap_id, reference_submap_ptr));
    CHECK(tsdf_submap_collection_ptr_->getTsdfSubmapConstPtrById(reading_submap_id, reading_submap_ptr));

    // Create problem and initial conditions
    ceres::Problem problem;
    ceres::LossFunction* loss_function = nullptr;

    // Create and add submap alignment cost function
    ceres::CostFunction* cost_function;
    if (options_.cost.cost_function_type == Options::CostFunction::Type::kNumeric) {
      // Create cost function with one residual per voxel
      RegistrationCostFunction* analytic_cost_function_ptr =
          new RegistrationCostFunction(reference_submap_ptr, reading_submap_ptr, options_.cost);
      cost_function = new ceres::NumericDiffCostFunction<RegistrationCostFunction,
                                                         ceres::CENTRAL,
                                                         ceres::DYNAMIC /* residuals */,
                                                         3 /* translation variables */>
                                                         (analytic_cost_function_ptr,
                                                          ceres::TAKE_OWNERSHIP,
                                                          int(analytic_cost_function_ptr->getNumRelevantVoxels()));
    } else {
      cost_function = new RegistrationCostFunction(reference_submap_ptr, reading_submap_ptr, options_.cost);
    }
    problem.AddResidualBlock(cost_function, loss_function, ref_t_ref_reading);

    // Run the solver
    ceres::Solver::Options ceres_options = options_.solver;
    ceres::Solve(ceres_options, &problem, &summary);

    return summary.IsSolutionUsable();
  }
} // namespace voxgraph
