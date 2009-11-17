
// 
// Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
//
// Authors:
//	Avadh Patel
//	Furat Afram
//

#ifndef _MEMORYSYSTEM_H_
#define _MEMORYSYSTEM_H_

#include <globals.h>
#include <superstl.h>

#include <ptlsim.h>
#include <memoryRequest.h>
#include <controller.h>
#include <interconnect.h>

//#define DEBUG_MEMORY
//#define DEBUG_WITH_FILE_NAME
#define ENABLE_CHECKS

#ifdef DEBUG_MEMORY
#ifdef DEBUG_WITH_FILE_NAME
#define memdebug(...) if(config.loglevel >= 5) { \
	ptl_logfile << __FILE__, ":", __LINE__,":\t", \
	__VA_ARGS__ ; ptl_logfile.flush(); }
#else
#define memdebug(...) if(config.loglevel >= 5) { \
	ptl_logfile << __VA_ARGS__ ; } //ptl_logfile.flush(); 
#endif
#else
#define memdebug(...) (0)
#endif

#ifndef ENABLE_CHECKS
#undef assert
#define assert(x) (x)
#endif

#define GET_STRINGBUF_PTR(var_name, ...)  \
	stringbuf *var_name = new stringbuf(); \
	*var_name << __VA_ARGS__; \
	

namespace OutOfOrderModel {
  class OutOfOrderMachine;
  class OutOfOrderCore;
  class LoadStoreQueueEntry;
  struct OutOfOrderCoreCacheCallbacks;
};

namespace Memory {

  using namespace OutOfOrderModel;

//  class Event : public FixQueueLinkObject
  class Event : public FixStateListObject
	{
		private:
			Signal *signal_;
			W64    clock_;
			void   *arg_;
		public:
//			Event(Signal *signal, W64 clock, void *arg) {
//				signal_ = signal;
//				clock_ = clock;
//				arg_ = arg;
//				next = null;
//				prev = null;
//			}

			void init() {
				signal_ = null;
				clock_ = -1;
				arg_ = null;
			}

			void setup(Signal *signal, W64 clock, void *arg) {
				signal_ = signal;
				clock_ = clock;
				arg_ = arg;
			}

			bool execute() {
				return signal_->emit(arg_);
			}

			W64 get_clock() {
				return clock_;
			}

//			void insert_after(Event* event) {
//				Event* next_event = (Event*)(this->next);
//				this->next = (selfqueuelink*)(event);
//				event->prev = (selfqueuelink*)(this);
//				if(next_event) {
//					event->next = (selfqueuelink*)next_event;
//					next_event->prev = (selfqueuelink*)event;
//				}
//			}
//
//			void insert_before(Event* event) {
//				Event* prev_event = (Event*)(this->prev);
//				this->prev = (selfqueuelink*)event;
//				event->next = (selfqueuelink*)this;
//				if(prev_event) {
//					prev_event->next = (selfqueuelink*)event;
//					event->prev = (selfqueuelink*)prev_event;
//				}
//			}
			
			ostream& print(ostream& os) const {
				os << "Event< ";
				if(signal_)
					os << "Signal:" << signal_->get_name() << " ";
				os << "Clock:" << clock_ << " ";
				os << "arg:" << arg_ ;
				os << ">" << endl, flush;
				return os;
			}
			
			bool operator ==(Event &event) {
				if(clock_ == event.clock_)
					return true;
				return false;
			}
			
			bool operator >(Event &event) {
				if(clock_ > event.clock_)
					return true;
				return false;
			}
			
			bool operator <(Event &event) {
				if(clock_ < event.clock_)
					return true;
				return false;
			}
	};

  ostream& operator <<(ostream& os, const Event& event);
  ostream& operator ,(ostream& os, const Event& event);

  
  //
  // MemoryHierarchy provides interface with core
  //
  
  class MemoryHierarchy {
  public:
    MemoryHierarchy(OutOfOrderMachine& machine);
    ~MemoryHierarchy(); // release memory for pool

	// check L1 availability
    bool is_cache_available(W8 coreid, W8 threadid, bool is_icache); 

    // interface to memory hierarchy
    bool access_cache(W8 coreid,		/* core */
                      W8 threadid,		/* thread */
                      int robid,		/* rob needed to wakeup dcache read */
                      W64 owner_uuid,	/* for debugging */
                      W64 owner_timestamp, /* for debugging */
                      W64 physaddr,		/* phyical address */
                      bool is_icache,	/* access icache if true */
                      bool is_write);	/* store request if true */

    // callback with response
    void icache_wakeup_wrapper(int coreid, W64 physaddr);
    void dcache_wakeup_wrapper(int coreid, 
                               int threadid,
                               int rob_idx,
                               W64 seq,
                               W64 physaddr);

	// to remove the requests if rob eviction has occured
	void annul_request(W8 coreid,
			W8 threadid,
			int robid,
			W64 physaddr,
			bool is_icache,
			bool is_write);

    void clock();

    void reset(); 

	// return the number of cycle used to flush the caches
    int flush(); 

	// for debugging
    void dump_info(ostream& os); 
	void print_map(ostream& os);

	// Add event into event queue
	void add_event(Signal *signal, int delay, void *arg);

	MemoryRequest* get_free_request() {
		return requestPool_.get_free_request();
	}

	void set_controller_full(Controller* controller, bool flag);
	void set_interconnect_full(Interconnect* interconnect, bool flag);
	bool is_controller_full(Controller* controller);

	Message* get_message();
	void free_message(Message* msg);

	int get_core_pending_offchip_miss(W8 coreid);

  private:

    void setup_topology();
    void shared_L2_configuration();
    void private_L2_configuration();

    // machine
    OutOfOrderMachine &machine_;  
	
	// array of caches and memory
	dynarray<Controller*> cpuControllers_;
	dynarray<Controller*> allControllers_; 
	dynarray<Interconnect*> allInterconnects_;
	Controller* memoryController_;

	// array to indicate if controller or interconnect buffers 
	// are full or not
	dynarray<bool> cpuFullFlags_;
	dynarray<bool> controllersFullFlags_;
	dynarray<bool> interconnectsFullFlags_;
	bool someStructIsFull_;
	
    // number of cores
    int coreNo_; 

	// Request pool
	RequestPool requestPool_;

	// Message pool
	FixStateList<Message, 128> messageQueue_;

	// Event Queue
	FixStateList<Event, 256> eventQueue_;

	void sort_event_queue(Event *event);

  }; 

};

#endif //_MEMORYSYSTEM_H_
