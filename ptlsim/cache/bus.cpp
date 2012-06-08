
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
 * Copyright 2009 Furat Afram <fafram@cs.binghamton.edu>
 *
 */

#ifdef MEM_TEST
#include <test.h>
#else
#include <ptlsim.h>
#endif

#include <bus.h>
#include <memoryHierarchy.h>
#include <machine.h>

using namespace Memory;

BusInterconnect::BusInterconnect(const char *name,
		MemoryHierarchy *memoryHierarchy) :
	Interconnect(name,memoryHierarchy),
	busBusy_(false)
{
    memoryHierarchy_->add_interconnect(this);

    SET_SIGNAL_CB(name, "_Broadcast", broadcast_, &BusInterconnect::broadcast_cb);

    SET_SIGNAL_CB(name, "_Broadcast_Complete", broadcastCompleted_,
            &BusInterconnect::broadcast_completed_cb);

    SET_SIGNAL_CB(name, "_Data_Broadcast", dataBroadcast_, &BusInterconnect::data_broadcast_cb);

    SET_SIGNAL_CB(name, "_Data_Broadcast_Complete", dataBroadcastCompleted_,
            &BusInterconnect::data_broadcast_completed_cb);

	lastAccessQueue = 0;

    if(!memoryHierarchy_->get_machine().get_option(name, "latency", latency_)) {
        latency_ = BUS_BROADCASTS_DELAY;
    }

    if(!memoryHierarchy_->get_machine().get_option(name, "aribtrate_latency",
                arbitrate_latency_)) {
        arbitrate_latency_ = BUS_ARBITRATE_DELAY;
    }
}

void BusInterconnect::register_controller(Controller *controller)
{
	BusControllerQueue *busControllerQueue = new BusControllerQueue();
	busControllerQueue->controller = controller;

	// Set controller pointer in each queue entry
	BusQueueEntry *entry;
	foreach(i, busControllerQueue->queue.size()) {
		entry = (BusQueueEntry*)&busControllerQueue->queue[i];
		entry->controllerQueue = busControllerQueue;
		entry = (BusQueueEntry*)&busControllerQueue->dataQueue[i];
		entry->controllerQueue = busControllerQueue;
	}

	busControllerQueue->idx = controllers.count();
	controllers.push(busControllerQueue);
	lastAccessQueue = controllers[0];
}

int BusInterconnect::access_fast_path(Controller *controller,
				MemoryRequest *request)
{
	return -1;
}

void BusInterconnect::annul_request(MemoryRequest *request)
{
	foreach(i, controllers.count()) {
		BusQueueEntry *entry;
		foreach_list_mutable(controllers[i]->queue.list(),
				entry, entry_t, nextentry_t) {
			if(entry->request == request) {
				entry->annuled = true;
				controllers[i]->queue.free(entry);
			}
		}
	}
}

bool BusInterconnect::controller_request_cb(void *arg)
{
	Message *message = (Message*)arg;

	BusControllerQueue* busControllerQueue = NULL;
	foreach(i, controllers.count()) {
		if(controllers[i]->controller ==
				(Controller*)message->sender) {
			busControllerQueue = controllers[i];
		}
	}

    assert(busControllerQueue);

	if (busControllerQueue->queue.isFull()) {
		memdebug("Bus queue is full\n");
		return false;
	}

	BusQueueEntry *busQueueEntry;
	busQueueEntry = busControllerQueue->queue.alloc();
	if(busControllerQueue->queue.isFull()) {
		memoryHierarchy_->set_interconnect_full(this, true);
	}
	busQueueEntry->request = message->request;
	message->request->incRefCounter();
	busQueueEntry->hasData = message->hasData;


	if(!is_busy()) {
		// address bus
		marss_add_event(&broadcast_, 1, NULL);
		set_bus_busy(true);
	} else {
		memdebug("Bus is busy\n");
	}

	return true;
}

BusQueueEntry* BusInterconnect::arbitrate_round_robin()
{
	memdebug("BUS:: doing arbitration.. \n");
	BusControllerQueue *controllerQueue = NULL;
	int i;
	if(lastAccessQueue)
		i = lastAccessQueue->idx;
	else
		i = 0;

	do {
		i = (i + 1) % controllers.count();
		controllerQueue = controllers[i];

		if(controllerQueue->queue.count() > 0) {
			BusQueueEntry *queueEntry = (BusQueueEntry*)
				controllerQueue->queue.peek();
			assert(queueEntry);
			assert(!queueEntry->annuled);
			lastAccessQueue = controllerQueue;
			return queueEntry;
		}
	} while(controllerQueue != lastAccessQueue);

	return NULL;
}

bool BusInterconnect::broadcast_cb(void *arg)
{
	BusQueueEntry *queueEntry;
	if(arg != NULL)
		queueEntry = (BusQueueEntry*)arg;
	else {
		queueEntry = arbitrate_round_robin();
        marss_add_event(&broadcast_, arbitrate_latency_, queueEntry);
        return true;
    }

	if(queueEntry == NULL) { // nothing to broadcast
		set_bus_busy(false);
		return true;
	}

	// first check if any of the other controller's receive queue is
	// full or not
	// if its full the don't broadcast untill it has a free
	// entry and  pass the queue entry as argument to the broadcast
	// signal so next time it doesn't need to arbitrate
	bool isFull = false;
	foreach(i, controllers.count()) {
		if(controllers[i]->controller ==
				queueEntry->controllerQueue->controller)
			continue;
		isFull |= controllers[i]->controller->is_full(true);
	}
	if(isFull) {
		marss_add_event(&broadcast_,
				latency_, queueEntry);
		return true;
	}

	set_bus_busy(true);

	Message& message = *memoryHierarchy_->get_message();
	message.sender = this;
	message.request = queueEntry->request;
	message.hasData = queueEntry->hasData;

	Controller *controller = queueEntry->controllerQueue->controller;

	foreach(i, controllers.count()) {
		if(controller != controllers[i]->controller) {
			bool ret = controllers[i]->controller->
				get_interconnect_signal()->emit(&message);
			assert(ret);
		}
	}

	// Free the entry from queue
	if(!queueEntry->annuled) {
		queueEntry->controllerQueue->queue.free(queueEntry);
	}
	if(!queueEntry->controllerQueue->queue.isFull()) {
		memoryHierarchy_->set_interconnect_full(this, false);
	}
	queueEntry->request->decRefCounter();
	marss_add_event(&broadcastCompleted_,
			latency_, NULL);

	// Free the message
	memoryHierarchy_->free_message(&message);

	return true;
}

bool BusInterconnect::broadcast_completed_cb(void *arg)
{
	assert(is_busy());

	// call broadcast_cb that will check if any pending
	// requests are there or not
	broadcast_cb(NULL);

	return true ;
}

bool BusInterconnect::data_broadcast_cb(void *arg)
{
    return true;
}

bool BusInterconnect::data_broadcast_completed_cb(void *arg)
{
    return true;
}

void BusInterconnect::print_map(ostream& os)
{
	os << "Bus Interconnect: ", get_name(), endl;
	os << "\tconnected to: ", endl;

	foreach(i, controllers.count()) {
		os << "\t\tcontroller[i]: ",
			controllers[i]->controller->get_name(), endl;
	}
}

/**
 * @brief Dump Bus Configuration in YAML Format
 *
 * @param out YAML Object
 */
void BusInterconnect::dump_configuration(YAML::Emitter &out) const
{
	out << YAML::Key << get_name() << YAML::Value << YAML::BeginMap;

	YAML_KEY_VAL(out, "type", "interconnect");
	YAML_KEY_VAL(out, "latency", latency_);
	YAML_KEY_VAL(out, "arbitrate_latency", arbitrate_latency_);
	if (controllers.size() > 0)
		YAML_KEY_VAL(out, "per_cont_queue_size",
				controllers[0]->queue.size());

	out << YAML::EndMap;
}

struct BusBuilder : public InterconnectBuilder
{
    BusBuilder(const char* name) :
        InterconnectBuilder(name)
    { }

    Interconnect* get_new_interconnect(MemoryHierarchy& mem,
            const char* name)
    {
        return new BusInterconnect(name, &mem);
    }
};

BusBuilder busBuilder("bus");
