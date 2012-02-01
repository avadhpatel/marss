
#ifndef DECODER_STATS_H
#define DECODER_STATS_H

#include <statsBuilder.h>

struct DecoderStats : public Statable
{
    struct throughput : public Statable
    {
        StatObj<W64> basic_blocks;
        StatObj<W64> x86_insns;
        StatObj<W64> uops;
        StatObj<W64> bytes;

        throughput(Statable *parent)
            : Statable("throughput", parent)
              , basic_blocks("basic_blocks", this)
              , x86_insns("x86_insns", this)
              , uops("uops", this)
              , bytes("bytes", this)
        { }
    } throughput;

    StatArray<W64, DECODE_TYPE_COUNT> x86_decode_type;

    struct bb_decode_type : public Statable
    {
        StatObj<W64> all_insns_fast;
        StatObj<W64> some_complex_insns;

        bb_decode_type(Statable *parent)
            : Statable("bb_decode_type", parent)
              , all_insns_fast("all_insns_fast", this)
              , some_complex_insns("some_complex_insns", this)
        { }
    } bb_decode_type;

    struct page_crossings : public Statable
    {
        StatObj<W64> within_page;
        StatObj<W64> crosses_page;

        page_crossings(Statable *parent)
            : Statable("page_crossings", parent)
              , within_page("within_page", this)
              , crosses_page("crosses_page", this)
        { }
    } page_crossings;

    struct cache : public Statable 
    {
        StatObj<W64> count;
        StatObj<W64> inserts;
        StatArray<W64, INVALIDATE_REASON_COUNT> invalidates;

        cache(const char* name, Statable *parent)
            : Statable(name, parent)
              , count("count", this)
              , inserts("inserts", this)
              , invalidates("invalidates", this, invalidate_reason_names)
        { }
    };

    cache bbcache;
    cache pagecache;

    StatObj<W64> reclaim_rounds;

    DecoderStats(Statable *parent)
        : Statable("decode", parent)
          , throughput(this)
          , x86_decode_type("x86_decode_type", this, decode_type_names)
          , bb_decode_type(this)
          , page_crossings(this)
          , bbcache("bbcache", this)
          , pagecache("pagecache", this)
          , reclaim_rounds("reclaim_rounds", this)
    { }
};

#endif
