
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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <globals.h>
#include <superstl.h>
#include <memoryRequest.h>

namespace Memory {

class Interconnect;

struct Message : public FixStateListObject {
	void *sender;
	MemoryRequest *request;
	bool hasData;
	bool isShared;
	void *arg;

	ostream& print(ostream& os) const {
		if(sender == null) {
			os << "Free Message\n";
			return os;
		}
		os << "arg:[", arg, "] ";
		os << "Message: sender[" , sender, "] ";
		os << "request[", *request, "] ";
		os << "isShared[", isShared, "] ";
		os << "hasData[", hasData, "]\n";
		return os;
	}

	void init() {
		sender = null;
		request = null;
		hasData = false;
		arg = null;
	}
};

static inline ostream& operator <<(ostream& os, const Message& msg)
{
	return msg.print(os);
}

class MemoryHierarchy;

class Controller
{
	private:
		char *name_;
		Signal handle_request_;
		Signal handle_interconnect_;
		bool isPrivate_;

	public:
		MemoryHierarchy *memoryHierarchy_;
		W8 coreid_;

		Controller(W8 coreid, char *name,
				MemoryHierarchy *memoryHierarchy) :
			memoryHierarchy_(memoryHierarchy)
			, coreid_(coreid)
			, handle_request_("handle_request")
			, handle_interconnect_("handle_interconnect")
		{
			name_ = name;
			isPrivate_ = false;

			handle_request_.connect(signal_mem_ptr \
					(*this, &Controller::handle_request_cb));
			handle_interconnect_.connect(signal_mem_ptr \
					(*this, &Controller::handle_interconnect_cb));
		}

		virtual bool handle_request_cb(void* arg)=0;
		virtual bool handle_interconnect_cb(void* arg)=0;
		virtual int access_fast_path(Interconnect *interconnect,
				MemoryRequest *request)=0;
		virtual void print_map(ostream& os)=0;

		virtual void print(ostream& os) const =0;
		virtual bool is_full(bool fromInterconnect = false) const = 0;
		virtual void annul_request(MemoryRequest* request) = 0;

		int flush() {
			return 0;
		}

		int get_no_pending_request(W8 coreid) { assert(0); return 0; }

		Signal* get_interconnect_signal() {
			return &handle_interconnect_;
		}

		Signal* get_handle_request_signal() {
			return &handle_request_;
		}

		char* get_name() const {
			return name_;
		}

		void set_private(bool flag) {
			isPrivate_ = flag;
		}

		bool is_private() { return isPrivate_; }

};

static inline ostream& operator <<(ostream& os, const Controller&
		controller)
{
	controller.print(os);
	return os;
}

};

#endif // CONTROLLER_H
