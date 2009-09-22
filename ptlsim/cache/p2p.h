
// 
// Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
//
// Authors:
//	Avadh Patel
//	Furat Afram
//

#ifndef P2P_INTERCONNECT_H
#define P2P_INTERCONNECT_H

#include <interconnect.h>

namespace Memory {

class P2PInterconnect : public Interconnect
{
	private:
		Controller *controllers_[2];

		bool send_request(Controller *sender, MemoryRequest *request,
				bool hasData);

		Controller* get_other_controller(Controller *controller) {
			if(controller == controllers_[0])
				return controllers_[1];
			else if(controller == controllers_[1])
				return controllers_[0];
			else
				assert(0); // Should never happen
			return null;
		}


	public:
		P2PInterconnect(const char *name, MemoryHierarchy *memoryHierarchy);
		bool controller_request_cb(void *arg);
		void register_controller(Controller *controller);
		int access_fast_path(Controller *controller, 
				MemoryRequest *request);
		void print_map(ostream& os);

		void print(ostream& os) const {
			os << "--P2P Interconnect: ", get_name(), endl;
		}

		// P2P has 0 delay in sending message
		int get_delay() {
			return 0;
		}

		void annul_request(MemoryRequest *request) {
		}
};

static inline ostream& operator << (ostream& os, const P2PInterconnect&
		p2p) 
{
	p2p.print(os);
	return os;
}

};

#endif // P2P_INTERCONNECT_H
