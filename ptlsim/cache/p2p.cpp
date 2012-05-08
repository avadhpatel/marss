
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
#include <machine.h>

using namespace Memory;

/**
 * @brief Initialize the P2PInterconnect module instance
 *
 * @param name Unique name assigend to this interconnect instance
 * @param memoryHierarchy Pointer to global memory hierarchy
 */
P2PInterconnect::P2PInterconnect(const char *name,
		MemoryHierarchy *memoryHierarchy) :
	Interconnect(name, memoryHierarchy)
{
	controllers_[0] = NULL;
	controllers_[1] = NULL;

    memoryHierarchy->add_interconnect(this);
}

/**
 * @brief Register Controller module to this interconnect
 *
 * @param controller Controller module instance
 *
 * This interconnect is designed to connect only 2 controllers, so it must be
 * called no more than 2 times.
 */
void P2PInterconnect::register_controller(Controller *controller)
{
	if(controllers_[0] == NULL) {
		controllers_[0] = controller;
		return;
	} else if(controllers_[1] == NULL) {
		controllers_[1] = controller;
		return;
	}

	memdebug("Already two controllers register in P2P\n");
    /* Already two controllers are registered */
	assert(0);
}

/**
 * @brief Controller Request entry point
 *
 * @param arg Message sent from Controller containing request
 *
 * @return True if message is successfully forwared else False
 */
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

/**
 * @brief Provides interface to fast access another controller
 *
 * @param controller Sender
 * @param request Memory Request
 *
 * @return Access cycle delay for fast path access
 */
int P2PInterconnect::access_fast_path(Controller *controller,
		MemoryRequest *request)
{
	Controller *receiver = get_other_controller(controller);
	return receiver->access_fast_path(this, request);
}

/**
 * @brief Print connections of this instance
 *
 * @param os Output string stream
 */
void P2PInterconnect::print_map(ostream &os)
{
	os << "Interconnect: " , get_name(), endl;
	os << "\tconntected to:", endl;

	if(controllers_[0] == NULL)
		os << "\t\tcontroller-1: None", endl;
	else
		os << "\t\tcontroller-1: ", controllers_[0]->get_name(), endl;

	if(controllers_[1] == NULL)
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

/**
 * @brief Dump P2P Configuration in YAML Format
 *
 * @param out YAML Object
 */
void P2PInterconnect::dump_configuration(YAML::Emitter &out) const
{
	out << YAML::Key << get_name() << YAML::Value << YAML::BeginMap;

	YAML_KEY_VAL(out, "type", "interconnect");
	/* Currently we have 0 latency */
	YAML_KEY_VAL(out, "latency", 0);

	out << YAML::EndMap;
}

/**
 * @brief P2P Interconnect Builder to export P2P interconnect to machine
 *
 * This builder creates an Interconnect module named 'p2p' that can be used in
 * machine configuration file.
 */
struct P2PBuilder : public InterconnectBuilder
{
    P2PBuilder(const char* name) :
        InterconnectBuilder(name)
    { }

    Interconnect* get_new_interconnect(MemoryHierarchy& mem,
            const char* name)
    {
        return new P2PInterconnect(name, &mem);
    }
};

P2PBuilder p2pBuilder("p2p");
