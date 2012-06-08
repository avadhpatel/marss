
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

#ifndef COHERENT_CACHE_H
#define COHERENT_CACHE_H

#include <logic.h>

#include <controller.h>
#include <interconnect.h>
#include <cacheConstants.h>
#include <memoryStats.h>
#include <statsBuilder.h>
#include <cacheLines.h>

namespace Memory {

    namespace CoherentCache {

        class CoherenceLogic;

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
                int waitFor;
                W64 dependsAddr;

                bitvec<CACHE_NO_EVENTS> eventFlags;

                Interconnect  *sender;
                Interconnect  *sendTo;
                MemoryRequest *request;
                Controller    *source;
                Controller    *dest;
                CacheLine     *line;
                void *m_arg;
                bool annuled;
                bool evicting;
                bool isSnoop;
                bool isShared;
                bool responseData;

                void init() {
                    request      = NULL;
                    sender       = NULL;
                    sendTo       = NULL;
                    line         = NULL;
                    m_arg        = NULL;
                    depends      = -1;
                    waitFor      = -1;
                    dependsAddr  = -1;
                    annuled      = false;
                    evicting     = false;
                    isSnoop      = false;
                    isShared     = false;
                    responseData = false;
                    source       = NULL;
                    dest         = NULL;
                    eventFlags.reset();
                }

                /* This is not created as copy constructor because
                 * there are some variables from parent class that
                 * we don't want to copy in this function. */
                void copy(CacheQueueEntry* ent)
                {
                    request = ent->request;
                    sender = ent->sender;
                    sendTo = ent->sendTo;
                    dest = ent->dest;
                    source = ent->source;
                    m_arg = ent->m_arg;
                    request->incRefCounter();
                }

                ostream& print(ostream& os) const {
                    if(!request) {
                        os << "Free Request Entry";
                        return os;
                    }

                    os << "Request{" << *request << "} ";

                    os << "idx["<< this->idx <<"] ";
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
                    os << "] waitFor[" << waitFor;
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

                CacheType       type_;
                CacheLinesBase *cacheLines_;

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

                Controller *directory_;
                Controller *lowerCont_;

                // All signals of cache
                Signal clearEntry_;
                Signal cacheHit_;
                Signal cacheMiss_;
                Signal cacheAccess_;
                Signal cacheInsert_;
                Signal cacheUpdate_;
                Signal cacheInsertComplete_;
                Signal waitInterconnect_;

                // Stats Objects
                MESIStats *new_stats;

                CoherenceLogic *coherence_logic_;

                CacheQueueEntry* find_dependency(MemoryRequest *request);

                // This function is used to find pending request with either
                // same MemoryRequest or memory request with same address
                CacheQueueEntry* find_match(MemoryRequest *request);

                W64 get_line_address(MemoryRequest *request) {
                    return request->get_physical_address() >> cacheLineBits_;
                }

                bool handle_upper_interconnect(Message &message);

                bool handle_lower_interconnect(Message &message);

                void handle_cache_insert(CacheQueueEntry *queueEntry,
                        W64 oldTag);
                bool is_line_valid(CacheLine *line);
                bool is_line_in_use(W64 tag);

                bool complete_request(Message &message, CacheQueueEntry
                        *queueEntry);

                void get_directory(Interconnect *interconn);

            public:
                CacheController(W8 coreid, const char *name,
                        MemoryHierarchy *memoryHierarchy, CacheType type);
                ~CacheController();
                bool handle_interconnect_cb(void *arg);
                int access_fast_path(Interconnect *interconnect,
                        MemoryRequest *request);
                void print_map(ostream& os);

                void register_interconnect(Interconnect *interconnect, int type);
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
						return (pendingRequests_.count() >= (
									pendingRequests_.size() - 6));
                    }
                    // Otherwise we keep 10 entries free for interconnect
                    // or some internal requests (for example, memory update
                    // requests are created internally)
                    if(pendingRequests_.count() >= (
                                pendingRequests_.size() - 20)) {
                        return true;
                    }
                    return false;
                }

                void annul_request(MemoryRequest *request);
				void dump_configuration(YAML::Emitter &out) const;

                // Callback functions for signals of cache
                virtual bool cache_hit_cb(void *arg);
                virtual bool cache_miss_cb(void *arg);
                virtual bool cache_access_cb(void *arg);
                virtual bool cache_insert_cb(void *arg);
                virtual bool cache_update_cb(void *arg);
                virtual bool cache_insert_complete_cb(void *arg);
                virtual bool wait_interconnect_cb(void *arg);
                virtual bool clear_entry_cb(void *arg);

                void set_coherence_logic(CoherenceLogic *cl) {
                    coherence_logic_ = cl;
                }

                Statable* get_stats() { return new_stats; }

                void send_message(CacheQueueEntry *queueEntry,
                        Interconnect *interconn, OP_TYPE type, W64 tag =-1);

                virtual void send_evict_to_upper(CacheQueueEntry *entry, W64 tag=-1);
                virtual void send_evict_to_lower(CacheQueueEntry *entry, W64 tag=-1);
                virtual void send_update_to_upper(CacheQueueEntry *entry, W64 tag=-1);
                virtual void send_update_to_lower(CacheQueueEntry *entry, W64 tag=-1);

                Interconnect* get_lower_intrconn() { return lowerInterconnect_;}
                Controller* get_directory() { return directory_; }
				Controller* get_lower_cont() { return lowerCont_; }
                CacheQueueEntry* get_new_queue_entry();

        };

    };

};

#endif // COHERENT_CACHE_H

