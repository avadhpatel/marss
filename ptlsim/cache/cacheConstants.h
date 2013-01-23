
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

#ifndef CACHECONSTANTS_H
#define CACHECONSTANTS_H

/*
 * Memory hierarchy constants
 */
namespace Memory{

	enum CacheType {
		L1_I_CACHE,
		L1_D_CACHE,
		L2_CACHE,
		L3_CACHE,
		MAIN_MEMORY
	};

	const int REQUEST_POOL_SIZE = 1024;
	const double REQUEST_POOL_LOW_RATIO = 0.1;

	/* CPU Controller */
	const int CPU_CONT_PENDING_REQ_SIZE = 128;
	const int CPU_CONT_ICACHE_BUF_SIZE = 32;

	/*
	 * Main memory outstanding queue size
	 * default size: 128
	 */
	const int MEM_REQ_NUM = 128;

	/*
	 * Main memory total bank number
	 * default: 64
	 * 2 Channels, 2 DIMMs per channel, 2 Ranks per DIMM, 8 Banks per Ranks --> total 64 banks
	 */
	const int MEM_BANKS = 64;

	/* Average wait dealy for retrying (general) */
	const int AVG_WAIT_DELAY = 5;
}
#endif // CACHECONSTANTS_H
