
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * This code is released under GPL.
 *
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#include <switch.h>

using namespace Memory;
using namespace Memory::SwitchInterconnect;


Switch::Switch(const char *name, MemoryHierarchy *memoryHierarchy)
    : Interconnect(name, memoryHierarchy)
{
    memoryHierarchy_->add_interconnect(this);

    SET_SIGNAL_CB(name, "_send", send, &Switch::send_cb);
    SET_SIGNAL_CB(name, "_send_complete", send_complete,
            &Switch::send_complete_cb);

    if(!memoryHierarchy_->get_machine().get_option(name, "latency", latency_)) {
        latency_ = SWITCH_DELAY;
    }
}

Switch::~Switch()
{
}

void Switch::register_controller(Controller *controller)
{
    ControllerQueue *cq = new ControllerQueue();
    cq->controller = controller;

    controllers.push(cq);
}

int Switch::access_fast_path(Controller *controller,
        MemoryRequest *request)
{
    return -1;
}

void Switch::annul_request(MemoryRequest *request)
{
    foreach (i, controllers.count()) {

        QueueEntry *entry;
        foreach_list_mutable (controllers[i]->queue.list(),
                entry, entry_t, nextentry_t) {

            if (entry->request->is_same(request)) {
                entry->annuled = true;
                entry->request->decRefCounter();
                ADD_HISTORY_REM(entry->request);
                controllers[i]->queue.free(entry);

                if (entry->in_use) {
                    /* If this entry is in use then clear its
                     * receiver controllers flag */
                    ControllerQueue* dq = get_queue(entry->dest);
                    dq->recv_busy = 0;
                }
            }
        }
    }
}

bool Switch::controller_request_cb(void *arg)
{
    Message *msg = (Message*)arg;

    ControllerQueue *cq = get_queue((Controller*)msg->sender);

    QueueEntry *queueEntry = cq->queue.alloc();

    if (!queueEntry) {
        return false;
    }

    *queueEntry << *msg;
    ADD_HISTORY_ADD(queueEntry->request);

    if (!cq->queue_in_use) {
        marss_add_event(&send, 1, cq);
        cq->queue_in_use = 1;
    }

    return true;
}

bool Switch::send_cb(void *arg)
{
    ControllerQueue *cq = (ControllerQueue*)arg;
    QueueEntry *queueEntry = cq->queue.head();

    if (queueEntry == NULL) {
        cq->queue_in_use = 0;
        return true;
    }

    /* Check if destination is available or not */
    ControllerQueue *dest_cq = get_queue(queueEntry->dest);

    if (queueEntry->annuled || dest_cq->recv_busy) {
        marss_add_event(&send, 2, cq);
        return true;
    }

    /* Set destination as busy and signal send_complete */
    queueEntry->in_use = 1;
    dest_cq->recv_busy = 1;
    marss_add_event(&send_complete, latency_, cq);

    return true;
}

bool Switch::send_complete_cb(void *arg)
{
    ControllerQueue *cq = (ControllerQueue*)arg;
    QueueEntry *queueEntry = cq->queue.head();

    if (queueEntry == NULL) {
        cq->queue_in_use = 0;
        return true;
    }

    if (queueEntry->annuled) {
        /* Try to send new packet arrived in queue */
        marss_add_event(&send, latency_, cq);
        return true;
    }

    ControllerQueue *dest_cq = get_queue(queueEntry->dest);

    Message *msg = memoryHierarchy_->get_message();
    msg->sender  = this;
    *msg << *queueEntry;

    bool success = dest_cq->controller->get_interconnect_signal()->
        emit(msg);

    memoryHierarchy_->free_message(msg);

    memdebug("Switch sending message success: " << success << endl);

	/* If destination is not accepting current controller's request
	 * so retry after AVG_WAIT_DELAY and meanwhile mark the
	 * destination controller as available , else on success
	 * remove the entry from queue. */

	if (success) {
		queueEntry->request->decRefCounter();
		ADD_HISTORY_REM(queueEntry->request);
		cq->queue.free(queueEntry);
	}

	dest_cq->recv_busy = 0;
	marss_add_event(&send, 1, cq);
	return true;
}

ControllerQueue* Switch::get_queue(Controller *cont)
{
    foreach (i, controllers.count()) {
        if (controllers[i]->controller == cont)
            return controllers[i];
    }

    assert(0);
    return NULL;
}

/**
 * @brief Dump Switch Interconnect Configuration in YAML Format
 *
 * @param out YAML Object
 */
void Switch::dump_configuration(YAML::Emitter &out) const
{
	out << YAML::Key << get_name() << YAML::Value << YAML::BeginMap;

	YAML_KEY_VAL(out, "type", "interconnect");
	YAML_KEY_VAL(out, "latency", latency_);
	if (controllers.size() > 0)
		YAML_KEY_VAL(out, "per_cont_queue_size",
				controllers[0]->queue.size());

	out << YAML::EndMap;
}

struct SwitchBuilder : public InterconnectBuilder
{
    SwitchBuilder(const char *name) :
        InterconnectBuilder(name)
    { }

    Interconnect* get_new_interconnect(MemoryHierarchy &mem,
            const char *name)
    {
        return new Switch(name, &mem);
    }
};

SwitchBuilder switchBuilder("switch");
