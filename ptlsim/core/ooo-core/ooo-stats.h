
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * This code is released under GPL.
 *
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#ifndef OOOCORE_STATS_H
#define OOOCORE_STATS_H

#include <ptlhwdef.h>
#include <branchpred.h>
#include <statsBuilder.h>
#include <ooo-const.h>
#include <decode.h>

namespace OOO_CORE_MODEL {

    struct OooCoreThreadStats : public Statable
    {
        struct fetch : public Statable
        {
            struct stop: public Statable
            {
                StatObj<W64> stalled;
                StatObj<W64> icache_miss;
                StatObj<W64> fetchq_full;
                StatObj<W64> issueq_quota_full;
                StatObj<W64> bogus_rip;
                StatObj<W64> microcode_assist;
                StatObj<W64> branch_taken;
                StatObj<W64> full_width;
                StatObj<W64> icache_stalled;
                StatObj<W64> invalid_blocks;

                stop(Statable *parent)
                    : Statable("stop", parent)
                      , stalled("stalled", this)
                      , icache_miss("icache_miss", this)
                      , fetchq_full("fetchq_full", this)
                      , issueq_quota_full("issueq_quota_full", this)
                      , bogus_rip("bogus_rip", this)
                      , microcode_assist("microcode_assist", this)
                      , branch_taken("branch_taken", this)
                      , full_width("full_width", this)
                      , icache_stalled("icache_stalled", this)
                      , invalid_blocks("invalid_blocks", this)
                {}
            } stop;

            StatArray<W64, OPCLASS_COUNT> opclass;
            StatArray<W64, FETCH_WIDTH+1> width;

            StatObj<W64> blocks;
            StatObj<W64> uops;
            StatObj<W64> user_insns;

            fetch(Statable *parent)
                : Statable("fetch", parent)
                  , stop(this)
                  , opclass("opclass", this, opclass_names)
                  , width("width", this)
                  , blocks("blocks", this)
                  , uops("uops", this)
                  , user_insns("user_insns", this)
            {}
        } fetch;

        struct frontend : public Statable
        {
            struct status : public Statable
            {
                StatObj<W64> complete;
                StatObj<W64> fetchq_empty;
                StatObj<W64> rob_full;
                StatObj<W64> physregs_full;
                StatObj<W64> ldq_full;
                StatObj<W64> stq_full;

                status(Statable *parent)
                    : Statable("status", parent)
                      , complete("complete", this)
                      , fetchq_empty("fetchq_empty", this)
                      , rob_full("rob_full", this)
                      , physregs_full("physregs_full", this)
                      , ldq_full("ldq_full", this)
                      , stq_full("stq_full", this)
                {
                   // complete.enable_periodic_dump();
                }
            } status;

            struct renamed : public Statable
            {
                StatObj<W64> none;
                StatObj<W64> reg;
                StatObj<W64> flags;
                StatObj<W64> reg_and_flags;

                renamed(Statable *parent)
                    : Statable("renamed", parent)
                      , none("none", this)
                      , reg("reg", this)
                      , flags("flags", this)
                      , reg_and_flags("reg_and_flags", this)
                {}
            } renamed;

            struct alloc : public Statable
            {
                StatObj<W64> reg;
                StatObj<W64> ldreg;
                StatObj<W64> sfr;
                StatObj<W64> br;

                alloc(Statable *parent)
                    : Statable("alloc", parent)
                      , reg("reg", this)
                      , ldreg("ldreg", this)
                      , sfr("sfr", this)
                      , br("br", this)
                {}
            } alloc;

            StatArray<W64, FRONTEND_WIDTH+1> width;
            StatArray<W64, 256> consumer_count;

            frontend(Statable *parent)
                : Statable("frontend", parent)
                  , status(this)
                  , renamed(this)
                  , alloc(this)
                  , width("width", this)
                  , consumer_count("consumer_count", this)
            {}
        } frontend;

        struct dispatch : public Statable
        {
            struct redispatch : public Statable
            {
                StatObj<W64> trigger_uops;
                StatObj<W64> deadlock_flushes;
                StatObj<W64> deadlock_uops_flushed;
                StatArray<W64, ROB_SIZE+1> dependent_uops;

                redispatch(Statable *parent)
                    : Statable("redispatch", parent)
                      , trigger_uops("trigger_uops", this)
                      , deadlock_flushes("deadlock_flushes", this)
                      , deadlock_uops_flushed("deadlock_uops_flushed", this)
                      , dependent_uops("dependent_uops", this)
                {}
            } redispatch;

            StatArray<W64, MAX_CLUSTERS> cluster;

            dispatch(Statable *parent)
                : Statable("dispatch", parent)
                  , redispatch(this)
                  , cluster("cluster", this, cluster_names)
            {}
        } dispatch;

        struct issue : public Statable
        {
            struct result : public Statable
            {
                StatObj<W64> no_fu;
                StatObj<W64> replay;
                StatObj<W64> misspeculated;
                StatObj<W64> refetch;
                StatObj<W64> branch_mispredict;
                StatObj<W64> exception;
                StatObj<W64> complete;

                result(Statable *parent)
                    : Statable("result", parent)
                      , no_fu("no_fu", this)
                      , replay("replay", this)
                      , misspeculated("misspeculated", this)
                      , refetch("refetch", this)
                      , branch_mispredict("branch_mispredict", this)
                      , exception("exception", this)
                      , complete("complete", this)
                {}
            } result;

            StatObj<W64> uops;
            StatEquation<W64, double, StatObjFormulaDiv> uipc;
            StatArray<W64, OPCLASS_COUNT> opclass;

            issue(Statable *parent)
                : Statable("issue", parent)
                  , result(this)
                  , uops("uops", this)
                  , uipc("uipc", this)
                  , opclass("opclass", this, opclass_names)
            {
                // Periodic Dump Sample: Uncomment following line to dump the
                // periodic stats of issued uop-types.
                // opclass.enable_periodic_dump();
            }
        } issue;

        struct writeback : public Statable
        {
            StatArray<W64, PHYS_REG_FILE_COUNT> writebacks;

            writeback(Statable *parent)
                : Statable("writeback", parent)
                  , writebacks("writebacks", this, phys_reg_file_names)
            {}
        } writeback;

        struct commit : public Statable
        {
            StatObj<W64> uops;
            StatObj<W64> insns;
            StatEquation<W64, double, StatObjFormulaDiv> uipc;
            StatEquation<W64, double, StatObjFormulaDiv> ipc;

            struct result : public Statable
            {
                StatObj<W64> none;
                StatObj<W64> ok;
                StatObj<W64> exception;
                StatObj<W64> skipblock;
                StatObj<W64> barrier_t;
                StatObj<W64> smc;
                StatObj<W64> memlocked;
                StatObj<W64> stop;
                StatObj<W64> dcache_stall;

                result(Statable *parent)
                    : Statable("result", parent)
                      , none("none", this)
                      , ok("ok", this)
                      , exception("exception", this)
                      , skipblock("skipblock", this)
                      , barrier_t("barrier", this)
                      , smc("smc", this)
                      , memlocked("memlocked", this)
                      , stop("stop", this)
                      , dcache_stall("dcache_stall", this)
                {}
            } result;

            struct fail : public Statable
            {
                StatObj<W64> free_list;
                StatObj<W64> frontend_list;
                StatObj<W64> ready_to_dispatch_list;
                StatObj<W64> dispatched_list;
                StatObj<W64> ready_to_issue_list;
                StatObj<W64> ready_to_store_list;
                StatObj<W64> ready_to_load_list;
                StatObj<W64> issued_list;
                StatObj<W64> completed_list;
                StatObj<W64> ready_to_writeback_list;
                StatObj<W64> cache_miss_list;
                StatObj<W64> tlb_miss_list;
                StatObj<W64> memory_fence_list;
                StatObj<W64> ready_to_commit_queue;

                fail(Statable *parent)
                    : Statable("fail", parent)
                      , free_list("free_list", this)
                      , frontend_list("frontend_list", this)
                      , ready_to_dispatch_list("ready_to_dispatch_list", this)
                      , dispatched_list("dispatched_list", this)
                      , ready_to_issue_list("ready_to_issue_list", this)
                      , ready_to_store_list("ready_to_store_list", this)
                      , ready_to_load_list("ready_to_load_list", this)
                      , issued_list("issued_list", this)
                      , completed_list("completed_list", this)
                      , ready_to_writeback_list("ready_to_writeback_list", this)
                      , cache_miss_list("cache_miss_list", this)
                      , tlb_miss_list("tlb_miss_list", this)
                      , memory_fence_list("memory_fence_list", this)
                      , ready_to_commit_queue("ready_to_commit_queue", this)
                {}
            } fail;

            struct setflags : public Statable
            {
                StatObj<W64> yes;
                StatObj<W64> no;

                setflags(Statable *parent)
                    : Statable("setflags", parent)
                      , yes("yes", this)
                      , no("no", this)
                {}
            } setflags;

            StatArray<W64, OPCLASS_COUNT> opclass;

            commit(Statable *parent)
                : Statable("commit", parent)
                  , uops("uops", this)
                  , insns("insns", this)
                  , uipc("uipc", this)
                  , ipc("ipc", this)
                  , result(this)
                  , fail(this)
                  , setflags(this)
                  , opclass("opclass", this, opclass_names)
            {
                ipc.enable_summary();
            }
        } commit;

        struct branchpred : public Statable
        {
            StatObj<W64> predictions;
            StatObj<W64> updates;

            // These counters are [0] = mispred, [1] = correct
            StatArray<W64, 2> cond;
            StatArray<W64, 2> indir;
            StatArray<W64, 2> ret;
            StatArray<W64, 2> summary;

            struct ras : public Statable
            {
                StatObj<W64> pushes;
                StatObj<W64> overflows;
                StatObj<W64> pops;
                StatObj<W64> underflows;
                StatObj<W64> annuls;

                ras(Statable *parent)
                    : Statable("ras", parent)
                      , pushes("pushes", this)
                      , overflows("overflows", this)
                      , pops("pops", this)
                      , underflows("underflows", this)
                      , annuls("annuls", this)
                {}
            } ras;

            branchpred(Statable *parent)
                : Statable("branchpred", parent)
                  , predictions("predictions", this)
                  , updates("updates", this)
                  , cond("cond", this, branchpred_outcome_names)
                  , indir("indir", this, branchpred_outcome_names)
                  , ret("ret", this, branchpred_outcome_names)
                  , summary("summary", this, branchpred_outcome_names)
                  , ras(this)
            {}
        } branchpred;

        struct dcache : public Statable
        {
            struct access_type : public Statable
            {
                struct issue : public Statable
                {
                    StatObj<W64> complete;
                    StatObj<W64> miss;
                    StatObj<W64> hit;
                    StatObj<W64> exception;
                    StatObj<W64> ordering;
                    StatObj<W64> unaligned;

                    struct replay : public Statable
                    {
                        StatObj<W64> sfr_addr_and_data_not_ready;
                        StatObj<W64> sfr_addr_not_ready;
                        StatObj<W64> sfr_data_not_ready;
                        StatObj<W64> missbuf_full;
                        StatObj<W64> interlocked;
                        StatObj<W64> interlock_overflow;
                        StatObj<W64> fence;
                        StatObj<W64> bank_conflict;
                        StatObj<W64> dcache_stall;
                        StatObj<W64> parallel_aliasing;

                        replay(Statable *parent)
                            : Statable("replay", parent)
                              , sfr_addr_and_data_not_ready("sfr_addr_and_data_not_ready", this)
                              , sfr_addr_not_ready("sfr_addr_not_ready", this)
                              , sfr_data_not_ready("sfr_data_not_ready", this)
                              , missbuf_full("missbuf_full", this)
                              , interlocked("interlocked", this)
                              , interlock_overflow("interlock_overflow", this)
                              , fence("fence", this)
                              , bank_conflict("bank_conflict", this)
                              , dcache_stall("dcache_stall", this)
                              , parallel_aliasing("parallel_aliasing", this)
                        {}
                    } replay;

                    issue(Statable *parent)
                        : Statable("issue", parent)
                          , complete("complete", this)
                          , miss("miss", this)
                          , hit("hit", this)
                          , exception("exception", this)
                          , ordering("ordering", this)
                          , unaligned("unaligned", this)
                          , replay(this)
                    {}
                } issue;

                struct forward : public Statable
                {
                    StatObj<W64> cache;
                    StatObj<W64> sfr;
                    StatObj<W64> zero;
                    StatObj<W64> sfr_and_cache;

                        forward(Statable *parent)
                            : Statable("forward", parent)
                              , cache("cache", this)
                              , sfr("sfr", this)
                              , zero("zero", this)
                              , sfr_and_cache("sfr_and_cache", this)
                    {}
                } forward;

                struct dependency : public Statable
                {
                    StatObj<W64> independent;
                    StatObj<W64> predicted_alias_unresolved;
                    StatObj<W64> stq_address_match;
                    StatObj<W64> stq_address_not_ready;
                    StatObj<W64> fence;
                    StatObj<W64> mmio;

                        dependency(Statable *parent)
                            : Statable("dependency", parent)
                              , independent("independent", this)
                              , predicted_alias_unresolved("predicted_alias_unresolved", this)
                              , stq_address_match("stq_address_match", this)
                              , stq_address_not_ready("stq_address_not_ready", this)
                              , fence("fence", this)
                              , mmio("mmio", this)
                    {}
                } dependency;

                struct type : public Statable
                {
                    StatObj<W64> aligned;
                    StatObj<W64> unaligned;
                    StatObj<W64> internal;

                        type(Statable *parent)
                            : Statable("type", parent)
                              , aligned("aligned", this)
                              , unaligned("unaligned", this)
                              , internal("internal", this)
                    {}
                } type;

                StatArray<W64, 4> size;

                StatArray<W64, DATATYPE_COUNT> datatype;

                access_type(const char *name, Statable *parent)
                    : Statable(name, parent)
                      , issue(this)
                      , forward(this)
                      , dependency(this)
                      , type(this)
                      , size("size", this, sizeshift_names)
                      , datatype("datatype", this, datatype_names)
                {}
            };

            access_type load;
            access_type store;

            struct fence : public Statable
            {
                StatObj<W64> lfence;
                StatObj<W64> sfence;
                StatObj<W64> mfence;
                
                fence(Statable *parent)
                    : Statable("fence", parent)
                      , lfence("lfence", this)
                      , sfence("sfence", this)
                      , mfence("mfence", this)
                {}
            } fence;

            struct tlb_stat : public Statable
            {
                StatObj<W64> hits;
                StatObj<W64> misses;

                tlb_stat(const char *name, Statable *parent)
                    : Statable(name, parent)
                      , hits("hits", this)
                      , misses("misses", this)
                {}
            };

            tlb_stat dtlb;
            tlb_stat itlb;

            StatArray<W64, 1001> dtlb_latency;
            StatArray<W64, 1001> itlb_latency;

            dcache(Statable *parent)
                : Statable("dcache", parent)
                  , load("load", this)
                  , store("store", this)
                  , fence(this)
                  , dtlb("dtlb", this)
                  , itlb("itlb", this)
                  , dtlb_latency("dtlb_latency", this)
                  , itlb_latency("itlb_latency", this)
            {}
        } dcache;

        StatObj<W64> interrupt_requests;
        StatObj<W64> cpu_exit_requests;
        StatObj<W64> cycles_in_pause;
        StatArray<W64, ASSIST_COUNT> assists;
        StatArray<W64, L_ASSIST_COUNT> lassists;

		StatArray<W64, PHYS_REG_FILE_COUNT> physreg_reads;
		StatArray<W64, PHYS_REG_FILE_COUNT> physreg_writes;

		StatObj<W64> rob_reads;
		StatObj<W64> rob_writes;

		StatObj<W64> rename_table_reads;
		StatObj<W64> rename_table_writes;

		StatObj<W64> reg_reads;
		StatObj<W64> reg_writes;
		StatObj<W64> fp_reg_reads;
		StatObj<W64> fp_reg_writes;

		StatObj<W64> ctx_switches;

		OooCoreThreadStats(const char *name, Statable *parent)
			: Statable(name, parent)
			  , fetch(this)
			  , frontend(this)
			  , dispatch(this)
			  , issue(this)
			  , writeback(this)
			  , commit(this)
			  , branchpred(this)
			  , dcache(this)
			  , interrupt_requests("interrupt_requests", this)
			  , cpu_exit_requests("cpu_exit_requests", this)
			  , cycles_in_pause("cycles_in_pause", this)
			  , assists("assists", this, assist_names)
			  , lassists("lassists", this, light_assist_names)
			  , physreg_reads("physreg_reads", this, phys_reg_file_names)
			  , physreg_writes("physreg_writes", this, phys_reg_file_names)
			  , rob_reads("rob_reads", this)
			  , rob_writes("rob_writes", this)
			  , rename_table_reads("rename_table_reads", this)
			  , rename_table_writes("rename_table_writes", this)
			  , reg_reads("reg_reads", this)
			  , reg_writes("reg_writes", this)
			  , fp_reg_reads("fp_reg_reads", this)
			  , fp_reg_writes("fp_reg_writes", this)
			  , ctx_switches("ctx_switches", this)
        {}
    };

    struct OooCoreStats
    {
        struct dispatch : public Statable
        {
            struct source : public Statable
            {
                StatArray<W64, MAX_PHYSREG_STATE> integer;
                StatArray<W64, MAX_PHYSREG_STATE> fp;
                StatArray<W64, MAX_PHYSREG_STATE> st;
                StatArray<W64, MAX_PHYSREG_STATE> br;

                source(Statable *parent)
                    : Statable("source", parent)
                      , integer("integer", this)
                      , fp("fp", this)
                      , st("st", this)
                      , br("br", this)
                {}
            } source;

            StatArray<W64, DISPATCH_WIDTH+1> width;
			StatArray<W64, OPCLASS_COUNT> opclass;

            dispatch(Statable *parent)
                : Statable("dispatch", parent)
                  , source(this)
                  , width("width", this)
				  , opclass("opclass", this, opclass_names)
            {}
        } dispatch;

        struct issue : public Statable
        {
            struct source : public Statable
            {
                StatArray<W64, MAX_PHYSREG_STATE> integer;
                StatArray<W64, MAX_PHYSREG_STATE> fp;
                StatArray<W64, MAX_PHYSREG_STATE> st;
                StatArray<W64, MAX_PHYSREG_STATE> br;

                source(Statable *parent)
                    : Statable("source", parent)
                      , integer("integer", this)
                      , fp("fp", this)
                      , st("st", this)
                      , br("br", this)
                {}
            } source;

            struct width : public Statable
            {
                StatArray<W64, MAX_ISSUE_WIDTH+1> int0;
                StatArray<W64, MAX_ISSUE_WIDTH+1> int1;
                StatArray<W64, MAX_ISSUE_WIDTH+1> fp;
                StatArray<W64, MAX_ISSUE_WIDTH+1> ld;
                StatArray<W64, MAX_ISSUE_WIDTH+1> all;

                width(Statable *parent)
                    : Statable("width", parent)
                      , int0("int0", this)
                      , int1("int1", this)
                      , fp("fp", this)
                      , ld("ld", this)
                      , all("all", this)
                {}
            } width;

            issue(Statable *parent)
                : Statable("issue", parent)
                  , source(this)
                  , width(this)
            {}
        } issue;

        struct writeback : public Statable
        {
            struct width : public Statable
            {
                StatArray<W64, MAX_ISSUE_WIDTH+1> int0;
                StatArray<W64, MAX_ISSUE_WIDTH+1> int1;
                StatArray<W64, MAX_ISSUE_WIDTH+1> fp;
                StatArray<W64, MAX_ISSUE_WIDTH+1> ld;
                StatArray<W64, MAX_ISSUE_WIDTH+1> all;

                width(Statable *parent)
                    : Statable("width", parent)
                      , int0("int0", this)
                      , int1("int1", this)
                      , fp("fp", this)
                      , ld("ld", this)
                      , all("all", this)
                {}
            } width;

            writeback(Statable *parent)
                : Statable("writeback", parent)
                  , width(this)
            {}
        } writeback;

        struct commit : public Statable
        {
            struct freereg : public Statable
            {
                StatObj<W64> pending;
                StatObj<W64> free;

                freereg(Statable *parent)
                    : Statable("freereg", parent)
                      , pending("pending", this)
                      , free("free", this)
                {}
            } freereg;

            StatObj<W64> free_reg_recycled;
            StatArray<W64, COMMIT_WIDTH+1> width;

            commit(Statable *parent)
                : Statable("commit", parent)
                  , freereg(this)
                  , free_reg_recycled("free_reg_recycled", this)
                  , width("width", this)
            {}
        } commit;

        StatObj<W64> cycles;

		StatObj<W64> iq_reads;
		StatObj<W64> iq_writes;
		StatObj<W64> iq_fp_reads;
		StatObj<W64> iq_fp_writes;

		OooCoreStats(const char *name, Statable *parent)
			: dispatch(parent)
			  , issue(parent)
			  , writeback(parent)
			  , commit(parent)
			  , cycles("cycles", parent)
			  , iq_reads("iq_reads", parent)
			  , iq_writes("iq_writes", parent)
			  , iq_fp_reads("iq_fp_reads", parent)
			  , iq_fp_writes("iq_fp_writes", parent)
		{ }
    };

};

#endif // OOOCORE_STATS_H
