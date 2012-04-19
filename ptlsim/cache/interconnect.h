
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

#ifndef INTERCONNECT_H
#define INTERCONNECT_H

#include <controller.h>

namespace Memory {

enum {
    INTERCONN_TYPE_UPPER = 0,
    INTERCONN_TYPE_UPPER2,
    INTERCONN_TYPE_LOWER,
    INTERCONN_TYPE_I,
    INTERCONN_TYPE_D,
    INTERCONN_TYPE_DIRECTORY,
};

class Interconnect
{
	private:
        stringbuf name_;
		Signal controller_request_;

	public:
		MemoryHierarchy *memoryHierarchy_;
		Interconnect(const char *name, MemoryHierarchy *memoryHierarchy)
			: controller_request_("Controller Request")
			, memoryHierarchy_(memoryHierarchy)
		{
			name_ << name;
			controller_request_.connect(signal_mem_ptr(*this,
						&Interconnect::controller_request_cb));
		}

        virtual ~Interconnect()
        {
            memoryHierarchy_ = NULL;
        }

		virtual bool controller_request_cb(void *arg)=0;
		virtual void register_controller(Controller *controller)=0;
		virtual int access_fast_path(Controller *controller,
				MemoryRequest *request)=0;
		virtual void print_map(ostream& os)=0;
		virtual void print(ostream& os) const = 0;
		virtual int get_delay()=0;
		virtual void annul_request(MemoryRequest* request) = 0;
		virtual void dump_configuration(YAML::Emitter &out) const = 0;

		Signal* get_controller_request_signal() {
			return &controller_request_;
		}

		char* get_name() const {
			return name_.buf;
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
