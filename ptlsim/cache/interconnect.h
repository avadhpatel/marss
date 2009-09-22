
// 
// Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
//
// Authors:
//	Avadh Patel
//	Furat Afram
//

#ifndef INTERCONNECT_H
#define INTERCONNECT_H

#include <controller.h>

namespace Memory {

class Interconnect
{
	private:
		const char *name_;
		Signal controller_request_;
		
	public:
		MemoryHierarchy *memoryHierarchy_;
		Interconnect(const char *name, MemoryHierarchy *memoryHierarchy) :
			memoryHierarchy_(memoryHierarchy)
			, controller_request_("Controller Request") 
		{
			name_ = name;
			controller_request_.connect(signal_mem_ptr(*this,
						&Interconnect::controller_request_cb));
		}
		virtual bool controller_request_cb(void *arg)=0;
		virtual void register_controller(Controller *controller)=0;
		virtual int access_fast_path(Controller *controller, 
				MemoryRequest *request)=0;
		virtual void print_map(ostream& os)=0;
		virtual void print(ostream& os) const = 0;
		virtual int get_delay()=0;
		virtual void annul_request(MemoryRequest* request) = 0;

		Signal* get_controller_request_signal() {
			return &controller_request_;
		}

		const char* get_name() const {
			return name_;
		}
};

static inline ostream& operator << (ostream& os, const Interconnect&
		inter)
{
	inter.print(os);
	return os;
}

};

#endif // INTERCONNECT_H
