#ifndef VOXGRAPH_SCAN_REGISTRATION_COST_FUNCTION_INL_H
#define VOXGRAPH_SCAN_REGISTRATION_COST_FUNCTION_INL_H

namespace voxgraph {
template<typename T>
bool ScanRegistrationCostFunction::operator()(const T *const t_S_O_estimate_ptr,
                                              const T *const q_S_O_estimate_ptr,
                                              T *residuals_ptr) const {
  // Wrap the data pointers into a minkindr transformation
  Eigen::Map<Eigen::Matrix<T, 6, 1>> residuals(residuals_ptr);
  Eigen::Map<const Eigen::Matrix<T, 3, 1>> t_S_O_estimate(t_S_O_estimate_ptr);
  Eigen::Map<const Eigen::Quaternion<T>> q_S_O_estimate(q_S_O_estimate_ptr);
  kindr::minimal::QuatTransformationTemplate<T> T_S_O(q_S_O_estimate,
                                                      t_S_O_estimate);

  // Iterate over all points
  sensor_msgs::PointCloud2ConstIterator<float> pointcloud_it(
      *pointcloud_msg_ptr_, "x");
  struct {
    bool triggered = false;
    unsigned int triggered_at_index = 0;
  } early_stop;
  for (unsigned int residual_idx = 0; residual_idx < points_per_pointcloud_;
       residual_idx++) {
    if (pointcloud_it != pointcloud_it.end()) {
      // Get the position of the current point P in submap frame S
      voxblox::Point t_C_P(pointcloud_it[0],
                           pointcloud_it[1],
                           pointcloud_it[2]);
      Eigen::Matrix<T, 3, 1> t_S_P = T_S_O * t_C_P.cast<T>();

      // Get the neighboring voxels
      const voxblox::TsdfVoxel *neighboring_voxels[8];
      Eigen::Matrix<T, 1, 8> jet_q_vector;
      bool lookup_succeeded =
          getVoxelsAndJetQVector(t_S_P, neighboring_voxels, &jet_q_vector);

      if (lookup_succeeded) {
        // Get the neighboring voxel distances
        voxblox::InterpVector neighboring_distances;
        voxblox::InterpVector neighboring_weights;
        for (int i = 0; i < neighboring_distances.size(); ++i) {
          neighboring_distances[i] = static_cast<voxblox::FloatingPoint>(
              neighboring_voxels[i]->distance);
          neighboring_weights[i] = static_cast<voxblox::FloatingPoint>(
              neighboring_voxels[i]->weight);
        }

        // Interpolate distance at point P
        const T interpolated_distance =
            jet_q_vector
                * (interp_table_.cast<T>()
                    * neighboring_distances.transpose().cast<T>());
//        const T interpolated_weight =
//            jet_q_vector
//                * (interp_table_.cast<T>()
//                    * neighboring_weights.transpose().cast<T>());
        // TODO(victorr): Also use the voxel weight
        residuals[residual_idx] = -interpolated_distance;  // * interpolated_weight;
      } else {
        residuals[residual_idx] = T(0.0); // no_correspondence_cost;
      }

      ++pointcloud_it;
    } else {
      // Log when we first ran out of points (i.e. size of the pointcloud)
      if (!early_stop.triggered) {
        early_stop.triggered = true;
        early_stop.triggered_at_index = residual_idx;
      }
      // Set all remaining residuals to zero
      residuals[residual_idx] = T(0.0);
    }
  }

  if (early_stop.triggered) {
    ROS_WARN_STREAM("Pointcloud only contained "
                    << early_stop.triggered_at_index
                    << " points, instead of the expected "
                    << points_per_pointcloud_);
  }

  return true;
}

template<typename T>
bool ScanRegistrationCostFunction::getVoxelsAndJetQVector(
    const Eigen::Matrix<T, 3, 1> &jet_pos,
    const voxblox::TsdfVoxel **voxels,
    Eigen::Matrix<T, 1, 8> *jet_q_vector) const {
  CHECK_NOTNULL(jet_q_vector);

  // get block and voxels indexes (some voxels may have negative indexes)
  voxblox::BlockIndex block_index;
  voxblox::InterpIndexes voxel_indexes;
  if (!tsdf_interpolator_.setIndexes(
      getScalarPart(jet_pos), &block_index, &voxel_indexes)) {
    return false;
  }

  // get distances of 8 surrounding voxels and weights vector
  // for each voxel index
  for (size_t i = 0; i < static_cast<size_t>(voxel_indexes.cols()); ++i) {
    typename voxblox::Layer<voxblox::TsdfVoxel>
    ::BlockType::ConstPtr block_ptr =
        submap_ptr_->getTsdfMap().getTsdfLayer().getBlockPtrByIndex(
            block_index);
    if (block_ptr == nullptr) {
      return false;
    }

    voxblox::VoxelIndex voxel_index = voxel_indexes.col(i);
    // if voxel index is too large get neighboring block and update index
    if ((voxel_index.array() >= block_ptr->voxels_per_side()).any()) {
      voxblox::BlockIndex new_block_index = block_index;
      for (size_t j = 0; j < static_cast<size_t>(block_index.rows()); ++j) {
        if (voxel_index(j) >=
            static_cast<voxblox::IndexElement>(block_ptr->voxels_per_side())) {
          new_block_index(j)++;
          voxel_index(j) -= block_ptr->voxels_per_side();
        }
      }
      block_ptr =
          submap_ptr_->getTsdfMap().getTsdfLayer().getBlockPtrByIndex(
              new_block_index);
      if (block_ptr == nullptr) {
        return false;
      }
    }
    // use bottom left corner voxel to compute weights vector
    if (i == 0) {
      // Compute the q_vector from paper http://spie.org/samples/PM159.pdf with
      // autodiff Jets
      const voxblox::Point voxel_pos =
          block_ptr->computeCoordinatesFromVoxelIndex(voxel_index);
      const Eigen::Matrix<T, 3, 1> voxel_offset =
          (jet_pos - voxel_pos.cast<T>()) * T(voxel_size_inv_);

      // TODO(victorr): Add check to ensure that scalar part of all
      //                voxel_offsets is positive

      // clang-format off
      *jet_q_vector <<
                    T(1.0),
                    voxel_offset[0],
                    voxel_offset[1],
                    voxel_offset[2],
                    voxel_offset[0] * voxel_offset[1],
                    voxel_offset[1] * voxel_offset[2],
                    voxel_offset[2] * voxel_offset[0],
                    voxel_offset[0] * voxel_offset[1] * voxel_offset[2];
      // clang-format on
    }

    const voxblox::TsdfVoxel& voxel =
        block_ptr->getVoxelByVoxelIndex(voxel_index);

    voxels[i] = &voxel;
    if (!voxblox::utils::isObservedVoxel(voxel)) {
      return false;
    }
  }
}
}  // namespace voxgraph

#endif //VOXGRAPH_SCAN_REGISTRATION_COST_FUNCTION_INL_H
