/**
 * Track replication progress on followers.
 */

#ifndef RAFT_PROGRESS_H_
#define RAFT_PROGRESS_H_

#include "../include/raft.h"

/**
 * Create and initialize the array of progress objects used by the leader to
 * track followers. The match index will be set to zero, and the current last
 * index plus 1.
 */
int progress__create_array(struct raft *r);

/**
 * Re-build the progress array against a new configuration.
 */
int progress__update_array(struct raft *r,
                           const struct raft_configuration *configuration);

/**
 * Return true if a majority of voting servers have made_contact with us since
 * the last check, i.e. the recent_recv flag of the associated progress
 * object is true, and then reset the flag as well.
 */
bool progress__check_quorum(struct raft *r);

#endif /* RAFT_PROGRESS_H_ */
