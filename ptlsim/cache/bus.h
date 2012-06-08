
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

#ifndef BUS_H
#define BUS_H

#include <interconnect.h>

namespace Memory {

// Bus Dealys
const int BUS_ARBITRATE_DELAY = 1;
const int BUS_BROADCASTS_DELAY = 6;

struct BusControllerQueue;

struct BusQueueEntry : public FixStateListObject
{
	MemoryRequest *request;
	BusControllerQueue *controllerQueue;
	bool hasData;
	bool annuled;

	void init() {
		request = NULL;
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

        int latency_;
        int arbitrate_latency_;

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
		void dump_configuration(YAML::Emitter &out) const;

		// Bus delay in sending message is BUS_BROADCASTS_DELAY
		int get_delay() {
			return latency_;
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
