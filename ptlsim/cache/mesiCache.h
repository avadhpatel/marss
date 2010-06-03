
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

#ifndef MESI_CACHE_H
#define MESI_CACHE_H

#include <logic.h>

#include <controller.h>
#include <interconnect.h>
#include <cacheConstants.h>
#include <memoryStats.h>

#define UPDATE_MESI_TRANS_STATS(old_state, new_state, mode) \
    if(mode) { /* kernel mode */ \
        kernelStats_->mesi_stats.state_transition[(old_state << 2) | new_state]++; \
    } else { \
        userStats_->mesi_stats.state_transition[(old_state << 2) | new_state]++; \
    }

namespace Memory {

    namespace MESICache {

        // CacheLine : a single cache line for MESI coherenent cache

        enum MESICacheLineState {
            MESI_MODIFIED = 0,
            MESI_EXCLUSIVE,
            MESI_SHARED,
            MESI_INVALID,
            NO_MESI_STATES
        };

        enum MESITransations {
            MM=0, ME, MS, MI,
            EM, EE, ES, EI,
            SM, SE, SS, SI,
            IM, IE, IS, II
        };

        static const char* MESIStateNames[NO_MESI_STATES] = {
            "Invalid",
            "Exclusive",
            "Shared",
            "Modified"
        };

        struct CacheLine
        {
            W64 tag;
            MESICacheLineState state;

            void init(W64 tag_t) {
                tag = tag_t;
                state = MESI_INVALID;
            }

            void reset() {
                tag = -1;
                state = MESI_INVALID;
            }

            void invalidate() { reset(); }

            void print(ostream& os) const {
                os << "Cacheline: tag[", hexstring(tag, 48) , "] ";
                os << "state[", MESIStateNames[state], "] ";
            }
        };

        static inline ostream& operator <<(ostream& os, const CacheLine& line)
        {
            line.print(os);
            return os;
        }

        // A base struct to provide a pointer to CacheLines without any need
        // of a template
        struct CacheLinesBase
        {
            public:
                virtual void init()=0;
                virtual W64 tagOf(W64 address)=0;
                virtual int latency() const =0;
                virtual CacheLine* probe(MemoryRequest *request)=0;
                virtual CacheLine* insert(MemoryRequest *request,
                        W64& oldTag)=0;
                virtual int invalidate(MemoryRequest *request)=0;
                virtual bool get_port(MemoryRequest *request)=0;
                virtual void print(ostream& os) const =0;
        };

        template <int SET_COUNT, int WAY_COUNT, int LINE_SIZE, int LATENCY>
            class CacheLines : public CacheLinesBase,
            public AssociativeArray<W64, CacheLine, SET_COUNT,
            WAY_COUNT, LINE_SIZE>
        {
            private:
                int readPortUsed_;
                int writePortUsed_;
                int readPorts_;
                int writePorts_;
                W64 lastAccessCycle_;

            public:
                typedef AssociativeArray<W64, CacheLine, SET_COUNT,
                        WAY_COUNT, LINE_SIZE> base_t;
                typedef FullyAssociativeArray<W64, CacheLine, WAY_COUNT,
                        NullAssociativeArrayStatisticsCollector<W64,
                        CacheLine> > Set;

                CacheLines(int readPorts, int writePorts);
                void init();
                W64 tagOf(W64 address);
                int latency() const { return LATENCY; };
                CacheLine* probe(MemoryRequest *request);
                CacheLine* insert(MemoryRequest *request, W64& oldTag);
                int invalidate(MemoryRequest *request);
                bool get_port(MemoryRequest *reqest);
                void print(ostream& os) const;
        };

        template <int SET_COUNT, int WAY_COUNT, int LINE_SIZE, int LATENCY>
            static inline ostream& operator <<(ostream& os, const
                    CacheLines<SET_COUNT, WAY_COUNT, LINE_SIZE, LATENCY>&
                    cacheLines)
            {
                cacheLines.print(os);
                return os;
            }

        // L1D cache lines
        typedef CacheLines<L1D_SET_COUNT, L1D_WAY_COUNT, L1D_LINE_SIZE,
                L1D_LATENCY> L1DCacheLines;

        // L1I cache lines
        typedef CacheLines<L1I_SET_COUNT, L1I_WAY_COUNT, L1I_LINE_SIZE,
                L1I_LATENCY> L1ICacheLines;

        // L2 cache lines
        typedef CacheLines<L2_SET_COUNT, L2_WAY_COUNT, L2_LINE_SIZE,
                L2_LATENCY> L2CacheLines;

        // L3 cache lines
        typedef CacheLines<L3_SET_COUNT, L3_WAY_COUNT, L3_LINE_SIZE,
                L3_LATENCY> L3CacheLines;

        // Cache Events enum used for Queue entry flags
        enum {
            CACHE_HIT_EVENT=0,
            CACHE_MISS_EVENT,
            CACHE_ACCESS_EVENT,
            CACHE_INSERT_EVENT,
            CACHE_UPDATE_EVENT,
            CACHE_CLEAR_ENTRY_EVENT,
            CACHE_INSERT_COMPLETE_EVENT,
            CACHE_WAIT_INTERCONNECT_EVENT,
            CACHE_NO_EVENTS
        };

        // CacheQueueEntry
        // Cache has queue to maintain a list of pending requests
        // that this caches has received.

        struct CacheQueueEntry : public FixStateListObject
        {
            public:
                int depends;

                bitvec<CACHE_NO_EVENTS> eventFlags;

                Interconnect *sender;
                Interconnect *sendTo;
                MemoryRequest *request;
                CacheLine *line;
                bool annuled;
                bool evicting;
                bool isSnoop;
                bool isShared;
                bool responseData;

                void init() {
                    request = null;
                    sender = null;
                    sendTo = null;
                    line = null;
                    depends = -1;
                    eventFlags.reset();
                    annuled = false;
                    evicting = false;
                    isSnoop = false;
                    isShared = false;
                    responseData = false;
                }

                ostream& print(ostream& os) const {
                    if(!request) {
                        os << "Free Request Entry";
                        return os;
                    }

                    os << "Request{", *request, "} ";

                    if(sender)
                        os << "sender[" << sender->get_name() << "] ";
                    else
                        os << "sender[none] ";

                    if(sendTo)
                        os << "sendTo[" << sendTo->get_name() << "] ";
                    else
                        os << "sendTo[none] ";

                    if(line)
                        os << "line[" << *line << "] ";
                    else
                        os << "line[none]";

                    os << "depends[" << depends;
                    os << "] eventFlags[" << eventFlags;
                    os << "] annuled[" << annuled;
                    os << "] evicting[" << evicting;
                    os << "] isSnoop[" << isSnoop;
                    os << "] isShared[" << isShared;
                    os << "] responseData[" << responseData;
                    os << "] ";
                    os << endl;
                    return os;
                }
        };

        static inline ostream& operator <<(ostream& os, const CacheQueueEntry&
                entry)
        {
            return entry.print(os);
        }

        class CacheController : public Controller
        {
            private:

                CacheType type_;
                CacheLinesBase *cacheLines_;
                CacheStats *userStats_;
                CacheStats *totalUserStats_;
                CacheStats *kernelStats_;
                CacheStats *totalKernelStats_;

                // No of bits needed to find Cache Line address
                int cacheLineBits_;

                // Cache Access Latency
                int cacheAccessLatency_;

                // A Queue conatining pending requests for this cache
                FixStateList<CacheQueueEntry, 256> pendingRequests_;

                // Flag to indicate if this cache is lowest private
                // level cache
                bool isLowestPrivate_;

                // This caches are connected to only two interconnects
                // upper and lower interconnect.
                Interconnect *upperInterconnect_;
                Interconnect *lowerInterconnect_;
                // second upper interconnect used to create private L2
                // caches where L2 is connected to L1i and L1d
                Interconnect *upperInterconnect2_;


                // All signals of cache
                Signal clearEntry_;
                Signal cacheHit_;
                Signal cacheMiss_;
                Signal cacheAccess_;
                Signal cacheInsert_;
                Signal cacheUpdate_;
                Signal cacheInsertComplete_;
                Signal waitInterconnect_;

                CacheQueueEntry* find_dependency(MemoryRequest *request);

                // This function is used to find pending request with either
                // same MemoryRequest or memory request with same address
                CacheQueueEntry* find_match(MemoryRequest *request);

                W64 get_line_address(MemoryRequest *request) {
                    return request->get_physical_address() >> cacheLineBits_;
                }

                bool handle_upper_interconnect(Message &message);

                bool handle_lower_interconnect(Message &message);

                // MESI Protocol related functions
                MESICacheLineState get_new_state(CacheQueueEntry *queueEntry,
                        bool isShared);
                void handle_snoop_hit(CacheQueueEntry *queueEntry);
                void handle_local_hit(CacheQueueEntry *queueEntry);
                void handle_cache_insert(CacheQueueEntry *queueEntry,
                        W64 oldTag);
                bool is_line_valid(CacheLine *line);

                void complete_request(Message &message, CacheQueueEntry
                        *queueEntry);

                // This function is used to maintain the inclusive
                // property between L1 and L2/L3
                void send_evict_message(CacheQueueEntry *queueEntry,
                        W64 oldTag =-1);


            public:
                CacheController(W8 coreid, char *name,
                        MemoryHierarchy *memoryHierarchy, CacheType type);
                bool handle_request_cb(void *arg);
                bool handle_interconnect_cb(void *arg);
                int access_fast_path(Interconnect *interconnect,
                        MemoryRequest *request);
                void print_map(ostream& os);

                void register_upper_interconnect(Interconnect *interconnect);
                void register_lower_interconnect(Interconnect *interconnect);
                void register_second_upper_interconnect(Interconnect
                        *interconnect);

                void set_lowest_private(bool flag) {
                    isLowestPrivate_ = flag;
                }

                bool is_lowest_private() {
                    return isLowestPrivate_;
                }

                void print(ostream& os) const;

                bool is_full(bool fromInterconnect = false) const {
                    if(fromInterconnect) {
                        // We keep some free entries for interconnect
                        // so if the queue is 100% full then only
                        // return false else return true
                        assert(!pendingRequests_.isFull());
                        return pendingRequests_.isFull();
                    }
                    // Otherwise we keep 10 entries free for interconnect
                    // or some internal requests (for example, memory update
                    // requests are created internally)
                    if(pendingRequests_.count() >= (
                                pendingRequests_.size() - 10)) {
                        return true;
                    }
                    return false;
                }

                void annul_request(MemoryRequest *request);

                // Callback functions for signals of cache
                bool cache_hit_cb(void *arg);
                bool cache_miss_cb(void *arg);
                bool cache_access_cb(void *arg);
                bool cache_insert_cb(void *arg);
                bool cache_update_cb(void *arg);
                bool cache_insert_complete_cb(void *arg);
                bool wait_interconnect_cb(void *arg);
                bool clear_entry_cb(void *arg);

        };

    };

};

#endif // MESI_CACHE_H
