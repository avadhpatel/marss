
// 
// Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
//
// Authors:
//	Avadh Patel
//	Furat Afram
//

#ifndef BUS_H
#define BUS_H

#include <interconnect.h>

namespace Memory {

struct BusControllerQueue;

struct BusQueueEntry : public FixStateListObject
{
	MemoryRequest *request;
	BusControllerQueue *controllerQueue;
	bool hasData;
	bool annuled;

	void init() {
		request = null;
		hasData = false;
		annuled = false;
	}

	ostream& print(ostream& os) const {
		if(!request) {
			os << "Free Bus Queue Entry";
			return os;
		}
		os << "request{", *request, "} ";
		os << "hasData[", hasData, "]";
		return os;
	}
};

struct BusControllerQueue
{
	int idx;
	Controller *controller;
	FixStateList<BusQueueEntry, 16> queue;
	FixStateList<BusQueueEntry, 16> dataQueue;
};

class BusInterconnect : public Interconnect
{
	private:
		dynarray<BusControllerQueue*> controllers;
		BusControllerQueue* lastAccessQueue;
		bool busBusy_;
		bool dataBusBusy_;
		Signal broadcast_;
		Signal dataBroadcast_;
		Signal broadcastCompleted_;
		Signal dataBroadcastCompleted_;

		BusQueueEntry *arbitrate_round_robin();

	public:
		BusInterconnect(const char *name, MemoryHierarchy *memoryHierarchy);
		bool is_busy(){ return busBusy_; }
		void set_bus_busy(bool flag){
			busBusy_ = flag;
		}
		bool controller_request_cb(void *arg);
		void register_controller(Controller *controller);
		int access_fast_path(Controller *controller,
				MemoryRequest *request);
		void print_map(ostream& os);
		void annul_request(MemoryRequest *request);

		// Bus delay in sending message is BUS_BROADCASTS_DELAY
		int get_delay() {
			return BUS_BROADCASTS_DELAY;
		}

		void print(ostream& os) const {
			os << "--Bus-Interconnect: ", get_name(), endl;
			foreach(i, controllers.count()) {
				os << "Controller Queue: ", endl;
				os << controllers[i]->queue;
			}
		}

		// Signal callbacks
		bool broadcast_cb(void *arg);
		bool broadcast_completed_cb(void *arg);
		bool data_broadcast_cb(void *arg);
		bool data_broadcast_completed_cb(void *arg);
};

static inline ostream& operator <<(ostream& os, 
		const BusInterconnect& bus)
{
	bus.print(os);
	return os;
}

};

#endif // BUS_H
