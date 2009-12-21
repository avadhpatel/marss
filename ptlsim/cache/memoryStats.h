
/* 
 * PQRS : A Full System Computer-Architecture Simulator
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

			W64 redirects;

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
