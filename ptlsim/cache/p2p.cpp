
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
#define PTLSIM_PUBLIC_ONLY
#include <ptlhwdef.h>
#endif

#include <p2p.h>
#include <memoryHierarchy.h>

using namespace Memory;

P2PInterconnect::P2PInterconnect(char *name,
		MemoryHierarchy *memoryHierarchy) :
	Interconnect(name, memoryHierarchy)
{
	controllers_[0] = null;
	controllers_[1] = null;
}

void P2PInterconnect::register_controller(Controller *controller)
{
	if(controllers_[0] == null) {
		controllers_[0] = controller;
		return;
	} else if(controllers_[1] == null) {
		controllers_[1] = controller;
		return;
	}

	memdebug("Already two controllers register in P2P\n");
    /* Already two controllers are registered */
	assert(0);
}

bool P2PInterconnect::controller_request_cb(void *arg)
{
    /*
     * P2P is 0 latency interconnect so directly
     * pass it to next controller
     */
	Message *msg = (Message*)arg;

	Controller *receiver = get_other_controller(
			(Controller*)msg->sender);

	Message& message = *memoryHierarchy_->get_message();
	message.sender = (void *)this;
	message.request = msg->request;
	message.hasData = msg->hasData;
	message.arg = msg->arg;

	bool ret_val;
	ret_val = receiver->get_interconnect_signal()->emit((void *)&message);

    /* Free the message */
	memoryHierarchy_->free_message(&message);

	return ret_val;

}

int P2PInterconnect::access_fast_path(Controller *controller,
		MemoryRequest *request)
{
	Controller *receiver = get_other_controller(controller);
	return receiver->access_fast_path(this, request);
}

void P2PInterconnect::print_map(ostream &os)
{
	os << "Interconnect: " , get_name(), endl;
	os << "\tconntected to:", endl;

	if(controllers_[0] == null)
		os << "\t\tcontroller-1: None", endl;
	else
		os << "\t\tcontroller-1: ", controllers_[0]->get_name(), endl;

	if(controllers_[1] == null)
		os << "\t\tcontroller-2: None", endl;
	else
		os << "\t\tcontroller-2: ", controllers_[1]->get_name(), endl;
}

bool P2PInterconnect::send_request(Controller *sender,
		MemoryRequest *request, bool hasData)
{
	assert(0);
	return false;
}
