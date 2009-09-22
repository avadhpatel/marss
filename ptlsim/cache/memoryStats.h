
// 
// Copyright 2009 Avadh Patel <apatel@cs.binghamton.edu>
//
// Authors:
//	Avadh Patel
//	Furat Afram
//

#ifndef MEMORY_STATS_H
#define MEMORY_STATS_H

//#include <dcache.h>

#define STAT_UPDATE(expr) stats_->expr, totalStats_->expr

namespace Memory {

	struct CacheStats{ // rootnode:

		struct cpurequest {
			struct count{ //node: summable
				struct hit {
					struct read{  //node: summable
						struct hit { //node: summable
							W64 hit;
							W64 forward;
						} hit;
					} read;

					struct write{  //node: summable
						struct hit { //node: summable
							W64 hit;
							W64 forward;
						} hit;
					} write;
				} hit;

				struct miss { //node: summable
					W64 read;
					W64 write;
				} miss;
			} count;

			struct stall{ //node: summable
				struct read{ //node: summable
					W64 dependency;
					W64 cache_port;
					W64 buffer_full;
				} read;

				struct write{  //node: summable
					W64 dependency;
					W64 cache_port;
					W64 buffer_full;
				} write;  
			} stall;

		} cpurequest ;

		struct snooprequest {
			W64 hit;
			W64 miss;
		} snooprequest;

		W64 annul;
		W64 queueFull;

		struct latency{ //node: summable 
			W64 IF;
			W64 load;
			W64 store;
		} latency;

		struct lat_count{  //node: summable
			W64 IF;
			W64 load;
			W64 store;
		} lat_count;
	};

	struct PerCoreCacheStats { // rootnode:
		CacheStats CPUController;
		CacheStats L1I;
		CacheStats L1D;
		CacheStats L2;
		CacheStats L3;
	};
};

#endif // MEMORY_STATS_H
