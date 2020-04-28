#include "recv.h"

#include "assert.h"
#include "convert.h"
#include "entry.h"
#include "heap.h"
#include "log.h"
#include "membership.h"
#include "recv_append_entries.h"
#include "recv_append_entries_result.h"
#include "recv_install_snapshot.h"
#include "recv_request_vote.h"
#include "recv_request_vote_result.h"
#include "recv_timeout_now.h"
#include "string.h"
#include "tracing.h"

/* Set to 1 to enable tracing. */
#if 0
#define tracef(...) Tracef(r->tracer, __VA_ARGS__)
#else
#define tracef(...)
#endif

/* Dispatch a single RPC message to the appropriate handler. */
static int recvMessage(struct raft *r, struct raft_message *message)
{
    int rv = 0;

    if (message->type < RAFT_IO_APPEND_ENTRIES ||
        message->type > RAFT_IO_TIMEOUT_NOW) {
        tracef("received unknown message type type: %d", message->type);
        return 0;
    }

    /* tracef("%s from server %ld", message_descs[message->type - 1],
       message->server_id); */

    switch (message->type) {
        case RAFT_IO_APPEND_ENTRIES:
            rv = recvAppendEntries(r, message->server_id,
                                   message->server_address,
                                   &message->append_entries);
            if (rv != 0) {
                entryBatchesDestroy(message->append_entries.entries,
                                    message->append_entries.n_entries);
            }
            break;
        case RAFT_IO_APPEND_ENTRIES_RESULT:
            rv = recvAppendEntriesResult(r, message->server_id,
                                         message->server_address,
                                         &message->append_entries_result);
            break;
        case RAFT_IO_REQUEST_VOTE:
            rv = recvRequestVote(r, message->server_id, message->server_address,
                                 &message->request_vote);
            break;
        case RAFT_IO_REQUEST_VOTE_RESULT:
            rv = recvRequestVoteResult(r, message->server_id,
                                       message->server_address,
                                       &message->request_vote_result);
            break;
        case RAFT_IO_INSTALL_SNAPSHOT:
            rv = recvInstallSnapshot(r, message->server_id,
                                     message->server_address,
                                     &message->install_snapshot);
            break;
        case RAFT_IO_TIMEOUT_NOW:
            rv = recvTimeoutNow(r, message->server_id, message->server_address,
                                &message->timeout_now);
            break;
    };

    if (rv != 0 && rv != RAFT_NOCONNECTION) {
        /* tracef("recv: %s: %s", message_descs[message->type - 1],
                 raft_strerror(rv)); */
        return rv;
    }

    /* If there's a leadership transfer in progress, check if it has
     * completed. */
    if (r->transfer != NULL) {
        if (r->follower_state.current_leader.id == r->transfer->id) {
            membershipLeadershipTransferClose(r);
        }
    }

    return 0;
}

void recvCb(struct raft_io *io, struct raft_message *message)
{
    struct raft *r = io->data;
    int rv;
    if (r->state == RAFT_UNAVAILABLE) {
        switch (message->type) {
            case RAFT_IO_APPEND_ENTRIES:
                entryBatchesDestroy(message->append_entries.entries,
                                    message->append_entries.n_entries);
                break;
            case RAFT_IO_INSTALL_SNAPSHOT:
                raft_configuration_close(&message->install_snapshot.conf);
                raft_free(message->install_snapshot.data.base);
                break;
        }
        return;
    }
    rv = recvMessage(r, message);
    if (rv != 0) {
        convertToUnavailable(r);
    }
}

/* Bump the current term to the given value and reset our vote, persiting the
 * change to disk. */
static int bumpCurrentTerm(struct raft *r, raft_term term)
{
    int rv;

    assert(r != NULL);
    assert(term >= r->current_term);

    /* Save the new term to persistent store, resetting the vote. */
    rv = r->io->set_term(r->io, term);
    if (rv != 0) {
        return rv;
    }

    /* Update our cache too. */
    r->current_term = term;
    r->voted_for = 0;

    return 0;
}

int recvEnsureMatchingTerms(struct raft *r, raft_term term, int *match)
{
    int rv;

    assert(r != NULL);
    assert(match != NULL);

    if (term < r->current_term) {
        *match = -1;
        return 0;
    }

    /* From Figure 3.1:
     *
     *   Rules for Servers: All Servers: If RPC request or response contains
     *   term T > currentTerm: set currentTerm = T, convert to follower.
     *
     * From state diagram in Figure 3.3:
     *
     *   [leader]: discovers server with higher term -> [follower]
     *
     * From Section 3.3:
     *
     *   If a candidate or leader discovers that its term is out of date, it
     *   immediately reverts to follower state.
     */
    if (term > r->current_term) {
        char msg[128];
        sprintf(msg, "remote term %lld is higher than %lld -> bump local term",
                term, r->current_term);
        if (r->state != RAFT_FOLLOWER) {
            strcat(msg, " and step down");
        }
        tracef("%s", msg);
        rv = bumpCurrentTerm(r, term);
        if (rv != 0) {
            return rv;
        }
        if (r->state != RAFT_FOLLOWER) {
            /* Also convert to follower. */
            convertToFollower(r);
        }
        *match = 1;
    } else {
        *match = 0;
    }

    return 0;
}

int recvUpdateLeader(struct raft *r, const raft_id id, const char *address)
{
    assert(r->state == RAFT_FOLLOWER);

    r->follower_state.current_leader.id = id;

    /* If the address of the current leader is the same as the given one, we're
     * done. */
    if (r->follower_state.current_leader.address != NULL &&
        strcmp(address, r->follower_state.current_leader.address) == 0) {
        return 0;
    }

    if (r->follower_state.current_leader.address != NULL) {
        MyHeapFree(r->follower_state.current_leader.address);
    }
    r->follower_state.current_leader.address = MyHeapMalloc(strlen(address) + 1);
    if (r->follower_state.current_leader.address == NULL) {
        return RAFT_NOMEM;
    }
    strcpy(r->follower_state.current_leader.address, address);

    return 0;
}

#undef tracef
