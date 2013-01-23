
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

#ifndef P2P_INTERCONNECT_H
#define P2P_INTERCONNECT_H

#include <interconnect.h>

namespace Memory {

/**
 * @brief Point-to-Point un-buffered Interconnect class
 *
 * This interconnect model is very simple un-buffered model that connects two
 * controllers.  Think of this interconnect as set of wires that connect two
 * caches directly.  This model doesn't have any buffers and currently its
 * latency is set to 0 cycle.
 */
class P2PInterconnect : public Interconnect
{
	private:
		Controller *controllers_[2];

		bool send_request(Controller *sender, MemoryRequest *request,
				bool hasData);

		/**
		 * @brief Get the controller connected to other end
		 *
		 * @param controller Controller sending the message
		 *
		 * @return Controller connected to other end of wires
		 */
		Controller* get_other_controller(Controller *controller) {
			if(controller == controllers_[0])
				return controllers_[1];
			else if(controller == controllers_[1])
				return controllers_[0];
			else
				assert(0); // Should never happen
			return NULL;
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

		/**
		 * @brief Return delay of this interconnect
		 *
		 * @return 1 cycle delay
		 *
		 * This delay gives a hint to controller that it should wait minimum
		 * these amount of cycles before retry when interconnect is busy.
		 */
		int get_delay() {
			return 1;
		}

		void annul_request(MemoryRequest *request) {
		}

		void dump_configuration(YAML::Emitter &out) const;
};

static inline ostream& operator << (ostream& os, const P2PInterconnect&
		p2p)
{
	p2p.print(os);
	return os;
}

};

#endif // P2P_INTERCONNECT_H
