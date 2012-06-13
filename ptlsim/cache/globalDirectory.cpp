
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * This code is released under GPL.
 *
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#include <globalDirectory.h>

/* Local variables and functions */
static W16 line_bits = log2(DIR_LINE_SIZE);

static W64 get_line_addr(W64 addr)
{
    return addr >> line_bits;
}


/**
 * @brief Reset the directory entry
 */
void DirectoryEntry::reset()
{
    present.reset();
    tag   = -1;
    owner = -1;
    dirty = 0;
	locked = 0;
}

void DirectoryEntry::init(W64 tag_)
{
    tag   = tag_;
    dirty = 0;
    owner = -1;
	locked = 0;
    present.reset();
}

Directory::Directory()
{
    entries = new base_t();
}

DirectoryEntry* Directory::insert(MemoryRequest *req, W64& old_tag)
{
    W64 phys_addr = req->get_physical_address();
    DirectoryEntry* entry = entries->select(phys_addr, old_tag);

    return entry;
}

DirectoryEntry* Directory::probe(MemoryRequest *req)
{
    W64 phys_addr = req->get_physical_address();
    DirectoryEntry* entry = entries->probe(phys_addr);

    return entry;
}

int Directory::invalidate(MemoryRequest *req)
{
    return entries->invalidate(req->get_physical_address());
}

Directory* Directory::dir = NULL;
FixStateList<DirContBufferEntry, REQ_Q_SIZE>*
DirectoryController::pendingRequests_ = NULL;

/**
 * @brief Get the global directory
 *
 * @return reference to global Directory
 */
Directory& Directory::get_directory()
{
    if (dir == NULL) {
        dir = new Directory();
        DirectoryController::pendingRequests_ =
            new FixStateList<DirContBufferEntry, REQ_Q_SIZE>();
    }

    return *dir;
}

Controller* DirectoryController::controllers[NUM_SIM_CORES] = {0};
Controller* DirectoryController::lower_cont = NULL;

DirectoryController* DirectoryController::dir_controllers[NUM_SIM_CORES] = {0};

DirectoryController::DirectoryController(W8 idx, const char *name,
        MemoryHierarchy *memoryHierarchy)
    : Controller(idx, name, memoryHierarchy)
      , dir_(Directory::get_directory())
{
    memoryHierarchy_->add_cache_mem_controller(this);

    req_handlers[MEMORY_OP_READ]   = &DirectoryController::
        handle_read_miss;
    req_handlers[MEMORY_OP_WRITE]  = &DirectoryController::
        handle_write_miss;
    req_handlers[MEMORY_OP_UPDATE] = &DirectoryController::
        handle_update;
    req_handlers[MEMORY_OP_EVICT]  = &DirectoryController::
        handle_evict;

    SET_SIGNAL_CB(name, "_Read_miss", read_miss,
            &DirectoryController::read_miss_cb);
    SET_SIGNAL_CB(name, "_Write_miss", write_miss,
            &DirectoryController::write_miss_cb);
    SET_SIGNAL_CB(name, "_Update", update,
            &DirectoryController::update_cb);
    SET_SIGNAL_CB(name, "_Evict", evict,
            &DirectoryController::evict_cb);
    SET_SIGNAL_CB(name, "_Send_update", send_update,
            &DirectoryController::send_update_cb);
    SET_SIGNAL_CB(name, "_Send_evict", send_evict,
            &DirectoryController::send_evict_cb);
    SET_SIGNAL_CB(name, "_Send_response", send_response,
            &DirectoryController::send_response_cb);
    SET_SIGNAL_CB(name, "_Send_msg", send_msg,
            &DirectoryController::send_msg_cb);
}

bool DirectoryController::handle_interconnect_cb(void *arg)
{
    Message *message = (Message*)arg;
    MemoryRequest *request = message->request;

	if (is_full() && !find_entry(message->request)) {
		return false;
	}

    memdebug("DirCont["<< get_name() << "] received message: " <<
            *message << endl);

    return (this->*req_handlers[request->get_type()])(message);
}

void DirectoryController::register_interconnect(Interconnect *interconn,
        int type)
{
    assert(type == INTERCONN_TYPE_DIRECTORY);
    interconn_ = interconn;

    // Now we setup peer Controllers and Lower controller
    BaseMachine& machine = memoryHierarchy_->get_machine();

    foreach (i, machine.connections.count()) {
        ConnectionDef *conn_def = machine.connections[i];

        if (strcmp(conn_def->name.buf, interconn->get_name()) == 0) {
            foreach (j, conn_def->connections.count()) {
                SingleConnection *sg = conn_def->connections[j];
                Controller **cont    = NULL;

                switch (sg->type) {
                    case INTERCONN_TYPE_DIRECTORY:
                        break;
                    case INTERCONN_TYPE_UPPER:
                        /* This controller is lower in hierarchy */
                        cont = machine.controller_hash.get(sg->controller);
                        assert(cont);
                        if (lower_cont) {
                            assert(lower_cont == *cont);
                        } else {
                            lower_cont = *cont;
                        }
                        break;
                    case INTERCONN_TYPE_LOWER:
                        /* This controller is up in hierarchy */
                        cont = machine.controller_hash.get(sg->controller);
                        assert(cont);
                        controllers[(*cont)->idx]     = (*cont);
                        dir_controllers[(*cont)->idx] = this;
                        break;
                    case INTERCONN_TYPE_UPPER2:
                    case INTERCONN_TYPE_I:
                    case INTERCONN_TYPE_D:
                    default:
                        ptl_logfile << "ERROR: Global Directory does not support Shared L2 connection" << endl;
                        assert(0);
                }
            }

            break;
        }
    }

    assert(lower_cont);
}

bool DirectoryController::handle_read_miss(Message *msg)
{
    DirContBufferEntry* queueEntry = find_entry(msg->request);

    if (queueEntry) {
        memdebug("Dir  request has completed, waking up dependents " <<
                *queueEntry << endl);
        /* This request has completed.. So finalize it */
        queueEntry->entry->present.set(queueEntry->cont->idx);
        if (!queueEntry->shared) {
            queueEntry->entry->owner = queueEntry->cont->idx;
            queueEntry->entry->dirty = 0;
        }

        wakeup_dependent(queueEntry);
        ADD_HISTORY_REM(queueEntry->request);
        queueEntry->request->decRefCounter();
        pendingRequests_->free(queueEntry);
        return true;
    }

    queueEntry = add_entry(msg);

    if (!queueEntry)
        return false;

    DirContBufferEntry* dep_entry = find_dependent_enry(queueEntry->request);
    assert(dep_entry != queueEntry);
    if (dep_entry) {
        dep_entry->depends = queueEntry->idx;
        return true;
    }

    return read_miss_cb(queueEntry);
}

bool DirectoryController::handle_write_miss(Message *msg)
{
    DirContBufferEntry* queueEntry = find_entry(msg->request);

    if (queueEntry) {
        memdebug("Dir  request has completed, waking up dependents " <<
                *queueEntry << endl);
        /* This request has completed.. So finalize it */
        queueEntry->entry->present.set(queueEntry->cont->idx);
        queueEntry->entry->owner = queueEntry->cont->idx;
        queueEntry->entry->dirty = 1;

        wakeup_dependent(queueEntry);
        ADD_HISTORY_REM(queueEntry->request);
        queueEntry->request->decRefCounter();
        pendingRequests_->free(queueEntry);
        return true;
    }

    queueEntry = add_entry(msg);

    if (!queueEntry)
        return false;

    DirContBufferEntry* dep_entry = find_dependent_enry(queueEntry->request);
    assert(dep_entry != queueEntry);
    if (dep_entry) {
        dep_entry->depends = queueEntry->idx;
        return true;
    }

    return write_miss_cb(queueEntry);
}

bool DirectoryController::handle_update(Message *msg)
{
    DirContBufferEntry* queueEntry = find_entry(msg->request);

    if (!queueEntry) {
        queueEntry = add_entry(msg);
    }

    if (!queueEntry)
        return false;

    return update_cb(queueEntry);
}

bool DirectoryController::handle_evict(Message *msg)
{
    DirContBufferEntry* queueEntry = find_entry(msg->request);

    if (!queueEntry) {
        queueEntry = add_entry(msg);
    }

    if (!queueEntry)
        return false;

    return evict_cb(queueEntry);
}

bool DirectoryController::read_miss_cb(void *arg)
{
    DirContBufferEntry *queueEntry = (DirContBufferEntry*)arg;

    DirectoryEntry *dir_entry      = get_directory_entry(queueEntry->request);
    DirectoryController *sig_dir   = this;

    if (!dir_entry) {
        // Retry after 1 cycle
        marss_add_event(&read_miss, 1, queueEntry);
        return true;
    }

    assert(dir_entry);
    queueEntry->entry = dir_entry;

    memdebug("Read miss handling in Directory with entry: " <<
            *dir_entry << endl);

    if (dir_entry->dirty) {
        /* If owner is in same group then directly send message to
         * that controller, else let it writeback dirty line to lower
         * level cache and then complete this request. */
        sig_dir = dir_controllers[dir_entry->owner];

        if (sig_dir == this && dir_entry->owner != queueEntry->cont->idx) {
            queueEntry->responder = controllers[dir_entry->owner];
            marss_add_event(&send_response, DIR_ACCESS_DELAY,
                    queueEntry);
        } else {
            queueEntry->responder = lower_cont;
            marss_add_event(&sig_dir->send_update,
                    DIR_ACCESS_DELAY, queueEntry);
        }

        return true;
    }

    if (dir_entry->present.nonzero() &&
            dir_controllers[dir_entry->owner] == this) {
        // Set Owner as responder if its in local group, else
        // set lower cache as responder
        queueEntry->responder = controllers[dir_entry->owner];
    } else {
        queueEntry->responder = lower_cont;
    }

    if (dir_entry->present.test(queueEntry->cont->idx))
        queueEntry->responder = lower_cont;

    // Send response back
    marss_add_event(&send_response, DIR_ACCESS_DELAY,
            queueEntry);

    return true;
}

bool DirectoryController::write_miss_cb(void *arg)
{
    DirContBufferEntry *queueEntry = (DirContBufferEntry*)arg;
    int cont_id = queueEntry->cont->idx;

    DirectoryEntry *dir_entry = get_directory_entry(queueEntry->request);
    DirectoryController *sig_dir = this;

    if (!dir_entry || dir_entry->locked) {
        // Retry after 1 cycle
        marss_add_event(&write_miss, 1, queueEntry);
        return true;
    }

    assert(dir_entry);
    queueEntry->entry = dir_entry;

    memdebug("Write miss handling in Directory with entry: " <<
            *dir_entry << endl);

    if (dir_entry->present.iszero()) {
        // Line is not cached.
        queueEntry->responder = lower_cont;
        dir_entry->dirty      = 1;
        dir_entry->owner      = cont_id;
    } else if (!dir_entry->present.test(cont_id)) {
        // Its not present in requested cache
        queueEntry->responder = lower_cont;
        sig_dir               = dir_controllers[dir_entry->owner];
        marss_add_event(&sig_dir->send_evict,
                DIR_ACCESS_DELAY, queueEntry);
        return true;
    } else {
        // Check if it was present in only requested cache
        dir_entry->present.reset(cont_id);

        if (dir_entry->present.nonzero()) {
            // Send evict msg to other caches
            queueEntry->responder = lower_cont;
            sig_dir               = dir_controllers[dir_entry->owner];
            marss_add_event(&sig_dir->send_evict,
                    DIR_ACCESS_DELAY, queueEntry);
            return true;
        }

        dir_entry->dirty = 1;
        dir_entry->present.set(cont_id);

        /* This line was present in only requested controller so
         * we send response back to same controller and set hasData
         * to 1 that indicates that request is complete. */
        queueEntry->hasData = 1;
    }

    marss_add_event(&send_response,
            DIR_ACCESS_DELAY, queueEntry);

    return true;
}

bool DirectoryController::update_cb(void *arg)
{
    DirContBufferEntry *queueEntry = (DirContBufferEntry*)arg;

    DirectoryEntry *dir_entry = queueEntry->entry;

    /* If this update was not initiated by directory controller
     * then we get the directory entry */
    if (!dir_entry)
        dir_entry = get_directory_entry(queueEntry->request, 1);

    if (!dir_entry) {
        // Retry after 1 cycle
        marss_add_event(&update, 1, queueEntry);
        return true;
    }

    // assert(dir_entry->dirty);
    // dir_entry->dirty = 0;

    /*
     * If it was a cache miss that triggerred update, then
     * send response to original request
     */
    if (queueEntry->origin != -1) {
        DirContBufferEntry *origEntry = get_entry(queueEntry->origin);
        if (origEntry) {
            DirectoryController *sig_dir = dir_controllers[
                origEntry->cont->idx];
            marss_add_event(&sig_dir->send_response, 1,
                    origEntry);
        }
    }

    /* Remove the queue entry */
    wakeup_dependent(queueEntry);
    ADD_HISTORY_REM(queueEntry->request);
    queueEntry->request->decRefCounter();
    pendingRequests_->free(queueEntry);

    return true;
}

bool DirectoryController::evict_cb(void *arg)
{
    DirContBufferEntry *queueEntry = (DirContBufferEntry*)arg;

    DirectoryEntry *dir_entry = queueEntry->entry;

    /* If this eviction was not initiated by directory controller
     * then we get the directory entry , and if entry is not present
     * then we ignore this eviction.*/
    if (!dir_entry) {
        dir_entry = get_directory_entry(queueEntry->request, 1);
        if (!dir_entry) {
            // Remove this queue entry
            wakeup_dependent(queueEntry);
            ADD_HISTORY_REM(queueEntry->request);
            queueEntry->request->decRefCounter();
            pendingRequests_->free(queueEntry);

            return true;
        }
    }

    if (!dir_entry) {
        // Retry after 1 cycle
        marss_add_event(&evict, 1, queueEntry);
        return true;
    }

    int cont_id = queueEntry->cont->idx;

    dir_entry->present.reset(cont_id);

    if (dir_entry->owner == cont_id) {
        if (dir_entry->present.iszero()) {
            dir_entry->owner = -1;
            dir_entry->dirty = 0;
        } else {
            dir_entry->owner = dir_entry->present.lsb();
        }
    }

    if (dir_entry->present.iszero() && queueEntry->origin != -1) {
        // All cached lines are evicted.
        // If origin is present means, this was a cache_miss request
        DirContBufferEntry *origEntry = get_entry(queueEntry->origin);
        if (origEntry) {
            DirectoryController *sig_dir = dir_controllers[
                origEntry->cont->idx];
            marss_add_event(&sig_dir->send_response, 1,
                    origEntry);
        }
    }

    // Remove this queue entry
    wakeup_dependent(queueEntry);
    ADD_HISTORY_REM(queueEntry->request);
    queueEntry->request->decRefCounter();
    pendingRequests_->free(queueEntry);

    return true;
}

/**
 * @brief Send UPDATE request to cache
 *
 * @param arg Original Queue entry
 *
 * @return true on success
 *
 */
bool DirectoryController::send_update_cb(void *arg)
{
    DirContBufferEntry *queueEntry = (DirContBufferEntry*)arg;
    if (queueEntry->annuled)
        return true;

    DirContBufferEntry *newEntry = pendingRequests_->alloc();

    if (!newEntry) {
        marss_add_event(&send_update, 1,
                queueEntry);
        return true;
    }

    newEntry->request = memoryHierarchy_->get_free_request(
            queueEntry->request->get_coreid());
    newEntry->request->init(queueEntry->request);
    newEntry->request->incRefCounter();
    newEntry->request->set_op_type(MEMORY_OP_UPDATE);
    newEntry->entry  = queueEntry->entry;
    newEntry->origin = (queueEntry->cont) ? queueEntry->idx : -1;

    ADD_HISTORY_ADD(newEntry->request);

    newEntry->cont      = controllers[queueEntry->entry->owner];
    newEntry->responder = queueEntry->responder;
    send_msg_cb(newEntry);

    return true;
}

/**
 * @brief Send EVICT message to all controllers that has cached line
 *
 * @param arg Queue entry
 *
 * @return True on success
 */
bool DirectoryController::send_evict_cb(void *arg)
{
    DirContBufferEntry *queueEntry = (DirContBufferEntry*)arg;
    if (queueEntry->annuled)
        return true;

    /* Check if we have enough free entries in queue */
    if (pendingRequests_->remaining() <
            (int)queueEntry->entry->present.popcount()) {
        marss_add_event(&send_evict, 1, queueEntry);
        return true;
    }

	/* While handling this request, if all other cache lines are
	 * evicted then send response to this request. */
	if (queueEntry->entry->present.iszero()) {
		DirectoryController *sig_dir = dir_controllers[
			queueEntry->cont->idx];
		marss_add_event(&sig_dir->send_response, 1, queueEntry);
		return true;
	}

	queueEntry->entry->locked = 1;

    /* Now for each cached entry, send evict message to that
     * controller */
    foreach (i, NUM_SIM_CORES) {
        if (!queueEntry->entry->present.test(i))
            continue;

        DirContBufferEntry *newEntry = pendingRequests_->alloc();

        assert(newEntry);

        newEntry->request = memoryHierarchy_->get_free_request(
                queueEntry->request->get_coreid());
        newEntry->request->init(queueEntry->request);
        newEntry->request->incRefCounter();
        newEntry->request->set_op_type(MEMORY_OP_EVICT);
        newEntry->entry  = queueEntry->entry;
        newEntry->origin = (queueEntry->cont) ? queueEntry->idx : -1;

        ADD_HISTORY_ADD(newEntry->request);

        newEntry->cont      = controllers[i];
        newEntry->responder = this;
        dir_controllers[i]->send_msg_cb(newEntry);
    }

    if (queueEntry->free_on_success) {
        ADD_HISTORY_REM(queueEntry->request);
        queueEntry->request->decRefCounter();
        pendingRequests_->free(queueEntry);
    }

    return true;
}

bool DirectoryController::send_response_cb(void *arg)
{
    DirContBufferEntry *queueEntry = (DirContBufferEntry*)arg;
    if (queueEntry->annuled)
        return true;

    memdebug("Dir: Sending response: " << *queueEntry << endl);

    assert(queueEntry->entry);
    queueEntry->entry->present.reset(queueEntry->cont->idx);
    queueEntry->shared = queueEntry->entry->present.nonzero();
    queueEntry->entry->present.set(queueEntry->cont->idx);

	queueEntry->entry->locked = 0;

    if (queueEntry->request->get_type() == MEMORY_OP_WRITE) {
        queueEntry->entry->owner = queueEntry->cont->idx;
        queueEntry->entry->dirty = 1;
    }

    if (!queueEntry->shared &&
            queueEntry->request->get_type() == MEMORY_OP_READ) {
        /* When we have only one cached entry then we do assign
         * that as owner untill some writer to same cache line shows up */
        queueEntry->entry->owner = queueEntry->cont->idx;
        queueEntry->entry->dirty = 0;
    }

    queueEntry->free_on_success = 1;

    /* This is called when we have evicted other caches or
     * updated lower cache for read access. Now all we
     * do is send response back to original requestor. */

    // wakeup_dependent(queueEntry);

    send_msg_cb(queueEntry);

    return true;
}

/**
 * @brief Send message to interconnect and retry if failed
 *
 * @param arg Queue entry to use to send message
 *
 * @return
 */
bool DirectoryController::send_msg_cb(void *arg)
{
    DirContBufferEntry *queueEntry = (DirContBufferEntry*)arg;
    if (queueEntry->annuled)
        return true;

    Message& message  = *memoryHierarchy_->get_message();
    message.sender    = this;
    message.dest      = queueEntry->cont;
    message.request   = queueEntry->request;
    message.isShared  = queueEntry->shared;
    message.hasData   = queueEntry->hasData;
    message.arg       = (void*)queueEntry->responder;

    memdebug("Directory sending msg, queue entry: " <<
            *queueEntry << "\n");

    bool success = interconn_->get_controller_request_signal()->emit(&message);

    /* Free the message */
    memoryHierarchy_->free_message(&message);

    if (!success) {
        int delay = interconn_->get_delay();
        if (delay == 0) delay = AVG_WAIT_DELAY;
        if (queueEntry->request->get_type() == MEMORY_OP_EVICT)
            delay = 1;
        marss_add_event(&send_msg, delay, queueEntry);
        return true;
    }

    if (queueEntry->free_on_success) {
        ADD_HISTORY_REM(queueEntry->request);
        queueEntry->request->decRefCounter();
        wakeup_dependent(queueEntry);
        pendingRequests_->free(queueEntry);
    }

    return true;
}

DirContBufferEntry* DirectoryController::add_entry(Message *msg)
{
    DirContBufferEntry* queueEntry = pendingRequests_->alloc();

    if (pendingRequests_->isFull()) {
        memoryHierarchy_->set_controller_full(this, true);
    }

    if (!queueEntry) {
        return NULL;
    }

    queueEntry->request = msg->request;
    queueEntry->request->incRefCounter();
    queueEntry->cont = (Controller*)msg->origin;

    ADD_HISTORY_ADD(queueEntry->request);

    return queueEntry;
}

DirContBufferEntry* DirectoryController::get_entry(int idx)
{
    DirContBufferEntry* queueEntry;
    foreach_list_mutable(pendingRequests_->list(), queueEntry, entry,
            preventry) {
        if (idx == queueEntry->idx)
            return queueEntry;
    }

    return NULL;
}

DirContBufferEntry* DirectoryController::find_entry(MemoryRequest *req)
{
    DirContBufferEntry* queueEntry;
    foreach_list_mutable(pendingRequests_->list(), queueEntry, entry,
            preventry) {
        if (req == queueEntry->request)
            return queueEntry;
    }

    return NULL;
}

DirContBufferEntry* DirectoryController::find_dependent_enry(
        MemoryRequest *req)
{
    W64 line_addr = get_line_addr(req->get_physical_address());

    DirContBufferEntry* queueEntry;
    foreach_list_mutable(pendingRequests_->list(), queueEntry, entry,
            preventry) {

        if (req == queueEntry->request || queueEntry->annuled)
            continue;

        if (get_line_addr(queueEntry->request->get_physical_address())
                == line_addr) {
            while(queueEntry->depends >= 0) {
                if ((*pendingRequests_)[queueEntry->depends].annuled)
                    break;
                queueEntry = &(*pendingRequests_)[queueEntry->depends];
            }

            return queueEntry;
        }
    }

    return NULL;
}

DirectoryEntry* DirectoryController::get_directory_entry(
        MemoryRequest *req, bool must_present)
{
    DirectoryEntry *entry = dir_.probe(req);

    if (!entry && must_present) {
        W64 tag_t = dir_.tag_of(req->get_physical_address());
        foreach (i, REQ_Q_SIZE) {
            DirectoryEntry* d_entry = &dummy_entries[i];
            if (d_entry->tag == tag_t) {
                entry = d_entry;
                break;
            }
        }
        return entry;
    }

    if (!entry) {
        W64 old_tag = InvalidTag<W64>::INVALID;
        entry = dir_.insert(req, old_tag);
        assert(entry);

        /* If we are removing any entry with cached line then we
         * must send evict signal to those caches. */
        if ((old_tag != InvalidTag<W64>::INVALID && old_tag != (W64)-1) &&
                entry->present.nonzero()) {
            DirContBufferEntry *newEntry = pendingRequests_->alloc();

            assert(newEntry);

            newEntry->request = memoryHierarchy_->get_free_request(
                    req->get_coreid());
            newEntry->request->init(req);
            newEntry->request->incRefCounter();
            newEntry->request->set_physical_address(old_tag);
            newEntry->request->set_op_type(MEMORY_OP_EVICT);
            newEntry->entry = get_dummy_entry(entry, old_tag);
            newEntry->free_on_success = 1;

            ADD_HISTORY_ADD(newEntry->request);

            DirContBufferEntry *depEntry = find_dependent_enry(
                    newEntry->request);

            if (depEntry) {
                depEntry->depends    = newEntry->idx;
                depEntry->entry      = newEntry->entry;
                newEntry->wakeup_sig = &send_evict;
            } else {
                send_evict_cb(newEntry);
            }
        }

        entry->init(dir_.tag_of(req->get_physical_address()));
    }

    return entry;
}

/**
 * @brief Return a free dummy Directory Entry
 *
 * @return DirectoryEntry to be used for eviction
 *
 * These dummy entries are used for eviction. When a directory replaces
 * an old entry it uses this dummy entries to send evict signals to cache
 * controllers.
 */
DirectoryEntry* DirectoryController::get_dummy_entry(DirectoryEntry *entry,
        W64 old_tag)
{
    foreach (i, REQ_Q_SIZE) {
        DirectoryEntry* d_entry = &dummy_entries[i];
        if (d_entry->present.iszero()) {
            // This entry is free. So use it.
            d_entry->tag   = old_tag;
            d_entry->dirty = entry->dirty;
            d_entry->owner = entry->owner;

            foreach (j, NUM_SIM_CORES)
                d_entry->present.assign(j, entry->present[j]);

            return d_entry;
        }
    }

    /* We must have atleast one free entry */
    assert(0);
    return NULL;
}

void DirectoryController::wakeup_dependent(DirContBufferEntry *queueEntry)
{
    if (queueEntry->depends >= 0) {
        DirContBufferEntry *depEntry = &(*pendingRequests_)[
            queueEntry->depends];

        /* If dependent entry is annuled then dont process it */
        if (depEntry->free) return;

        Signal *sig = depEntry->wakeup_sig;

        if (!sig) {
            switch (depEntry->request->get_type()) {
                case MEMORY_OP_READ:   sig = &read_miss; break;
                case MEMORY_OP_WRITE:  sig = &write_miss; break;
                case MEMORY_OP_UPDATE: sig = &update; break;
                case MEMORY_OP_EVICT:  sig = &evict; break;
                default: assert(0);
            }
        }

        marss_add_event(sig, 1, depEntry);
    }
}

void DirectoryController::print_map(ostream &os)
{
    os << "Global Directory Controller: name[" << get_name();
    os << "] idx[" << idx << "]\n";
}

void DirectoryController::print(ostream &os) const
{
    os << "Global Directory Controller: " << get_name();
    os << " [" << idx << "]\n";

    os << "\tDirectory Controller: ";
    foreach (i, NUM_SIM_CORES) {
        os << "  [" << i << "] ";
        if (dir_controllers[i]) os << dir_controllers[i]->get_name();
        else os << "0";
    }
    os << endl;

    os << "\tControllers: ";
    foreach (i, NUM_SIM_CORES) {
        os << "  [" << i << "] ";
        if (controllers[i]) os << controllers[i]->get_name();
        else os << "0";
    }
    os << endl;

    if (lower_cont) os << "\tLower cont: " << lower_cont->get_name();
    else os << "\tLower cont: 0";

    os << endl;

    os << "Queue:\n" << *pendingRequests_ << endl;
}

bool DirectoryController::is_full(bool flag) const
{
    if (pendingRequests_->count() >= (
                pendingRequests_->size() - 10)) {
        return true;
    }
    return false;
}

void DirectoryController::annul_request(MemoryRequest *request)
{
    DirContBufferEntry *entry;
    foreach_list_mutable (pendingRequests_->list(), entry,
            entry_t, nextentry_t) {
        if (entry->request->is_same(request)) {
            entry->annuled = true;
            ADD_HISTORY_REM(entry->request);
            entry->request->decRefCounter();

            wakeup_dependent(entry);

            pendingRequests_->free(entry);
        }
    }
}

/**
 * @brief Dump Directory Configuration in YAML Format
 *
 * @param out YAML Object
 */
void DirectoryController::dump_configuration(YAML::Emitter &out) const
{
	out << YAML::Key << get_name() << YAML::Value << YAML::BeginMap;

	YAML_KEY_VAL(out, "type", "directory");
	YAML_KEY_VAL(out, "size", DIR_SET * DIR_WAY);
	YAML_KEY_VAL(out, "line_size", DIR_LINE_SIZE);
	YAML_KEY_VAL(out, "sets", DIR_SET);
	YAML_KEY_VAL(out, "ways", DIR_WAY);

	out << YAML::EndMap;
}

/**
 * @brief A Builder plugin for Global Directory Controller
 */
struct GlobalDirContBuilder : public ControllerBuilder
{
    GlobalDirContBuilder(const char *name):
        ControllerBuilder(name)
    {}

    Controller* get_new_controller(W8 idx, W8 type,
            MemoryHierarchy &mem, const char *name)
    {
        return new DirectoryController(idx, name, &mem);
    }
};

GlobalDirContBuilder globalDirBuilder("global_dir");
