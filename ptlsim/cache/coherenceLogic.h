
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
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#ifndef COHERENCE_LOGIC_H
#define COHERENCE_LOGIC_H

#include <logic.h>

#include <memoryHierarchy.h>
#include <controller.h>
#include <statsBuilder.h>
#include <cacheLines.h>

namespace Memory {

    class MemoryHierarchy;

    namespace CoherentCache {

        class CacheController;
        class CacheQueueEntry;

        class CoherenceLogic : public Statable
        {
            public:
                CoherenceLogic(const char*name, CacheController* cont,
                        Statable *parent, MemoryHierarchy* mem)
                    : Statable(name, parent)
                      , controller(cont)
                      , memoryHierarchy(mem)
            {}

                virtual void handle_local_hit(CacheQueueEntry *entry)      = 0;
                virtual void handle_local_miss(CacheQueueEntry *entry)     = 0;
                virtual void handle_interconn_hit(CacheQueueEntry *entry)  = 0;
                virtual void handle_interconn_miss(CacheQueueEntry *entry) = 0;
                virtual void handle_cache_insert(CacheQueueEntry *entry,
                        W64 oldTag)                                        = 0;
                virtual void handle_cache_evict(CacheQueueEntry *entry)    = 0;
                virtual void complete_request(CacheQueueEntry *entry,
                        Message &message)                                  = 0;
                virtual bool is_line_valid(CacheLine *line)                = 0;
                virtual void invalidate_line(CacheLine *line)              = 0;
                virtual void handle_response(CacheQueueEntry *entry,
                        Message &message) = 0;
				virtual void dump_configuration(YAML::Emitter &out) const = 0;

                CacheController* controller;
                MemoryHierarchy* memoryHierarchy;
        };
    };
};

#endif
