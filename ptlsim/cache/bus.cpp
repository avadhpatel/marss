
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

using namespace Memory;

BusInterconnect::BusInterconnect(char *name,
		MemoryHierarchy *memoryHierarchy) :
	Interconnect(name,memoryHierarchy),
	busBusy_(false)
{
	GET_STRINGBUF_PTR(broadcast_name, name, "_broadcast");
	broadcast_.set_name(broadcast_name->buf);
	broadcast_.connect(signal_mem_ptr(*this,
				&BusInterconnect::broadcast_cb));

	GET_STRINGBUF_PTR(broadcastComplete_name, name,
			"_broadcastComplete");
	broadcastCompleted_.set_name(broadcastComplete_name->buf);
	broadcastCompleted_.connect(signal_mem_ptr(*this,
				&BusInterconnect::broadcast_completed_cb));

	GET_STRINGBUF_PTR(dataBroadcast_name, name, "_dataBroadcast");
	dataBroadcast_.set_name(dataBroadcast_name->buf);
	dataBroadcast_.connect(signal_mem_ptr(*this,
				&BusInterconnect::data_broadcast_cb));

	GET_STRINGBUF_PTR(dataBroadcastComplete_name, name,
			"_dataBroadcastComplete");
	dataBroadcastCompleted_.set_name(dataBroadcastComplete_name->buf);
	dataBroadcastCompleted_.connect(signal_mem_ptr(*this,
				&BusInterconnect::data_broadcast_completed_cb));

	lastAccessQueue = 0;
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

	BusControllerQueue* busControllerQueue;
	foreach(i, controllers.count()) {
		if(controllers[i]->controller ==
				(Controller*)message->sender) {
			busControllerQueue = controllers[i];
		}
	}

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
		memoryHierarchy_->add_event(&broadcast_, 1, null);
		set_bus_busy(true);
	} else {
		memdebug("Bus is busy\n");
	}

	return true;
}

BusQueueEntry* BusInterconnect::arbitrate_round_robin()
{
	memdebug("BUS:: doing arbitration.. \n");
	BusControllerQueue *controllerQueue = null;
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

	return null;
}

bool BusInterconnect::broadcast_cb(void *arg)
{
	BusQueueEntry *queueEntry;
	if(arg != null)
		queueEntry = (BusQueueEntry*)arg;
	else
		queueEntry = arbitrate_round_robin();

	if(queueEntry == null) { // nothing to broadcast
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
		memoryHierarchy_->add_event(&broadcast_,
				BUS_BROADCASTS_DELAY, queueEntry);
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
	memoryHierarchy_->add_event(&broadcastCompleted_,
			BUS_BROADCASTS_DELAY, null);

	// Free the message
	memoryHierarchy_->free_message(&message);

	return true;
}

bool BusInterconnect::broadcast_completed_cb(void *arg)
{
	assert(is_busy());

	// call broadcast_cb that will check if any pending
	// requests are there or not
	broadcast_cb(null);

	return true ;
}

bool BusInterconnect::data_broadcast_cb(void *arg)
{
}

bool BusInterconnect::data_broadcast_completed_cb(void *arg)
{
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

