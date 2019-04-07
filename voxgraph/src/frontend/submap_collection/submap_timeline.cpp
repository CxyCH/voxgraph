//
// Created by victor on 06.03.19.
//

#include "voxgraph/frontend/submap_collection/submap_timeline.h"

namespace voxgraph {
void voxgraph::SubmapTimeline::addNextSubmap(
    const ros::Time &submap_creation_timestamp,
    const cblox::SubmapID &submap_id) {
  submap_timeline_.emplace_hint(submap_timeline_.end(),
                                submap_creation_timestamp, submap_id);
}

bool SubmapTimeline::lookupActiveSubmapByTime(const ros::Time &timestamp,
                                              cblox::SubmapID *submap_id) {
  CHECK_NOTNULL(submap_id);

  // Get an iterator to the end of the time interval in which timestamp falls
  auto iterator = submap_timeline_.upper_bound(timestamp);

  // Ensure that the timestamp is not from before we started logging
  if (iterator == submap_timeline_.begin()) {
    submap_id = nullptr;
    return false;
  }

  // The interval's active submap id is stored at its start point
  iterator--;
  *submap_id = iterator->second;
  return true;
}
}  // namespace voxgraph
