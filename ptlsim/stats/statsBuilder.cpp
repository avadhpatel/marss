
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * This code is released under GPL.
 *
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#include "statsBuilder.h"

#include <ptlsim.h>

static Stats *periodic_stats = NULL;
static Stats *temp_stats  = NULL;
static Stats *temp2_stats  = NULL;

Statable::Statable(const char *name)
{
    this->name = name;
    parent = NULL;
    summarize = false;
    default_stats = NULL;
    dump_disabled = false;
    periodic_enabled = false;

    StatsBuilder &builder = StatsBuilder::get();
    builder.add_to_root(this);
}

Statable::Statable(const char *name, bool is_root)
{
    this->name = name;
    parent = NULL;
    summarize = false;
    default_stats = NULL;
    dump_disabled = false;
    periodic_enabled = false;

    if(!is_root) {
        StatsBuilder &builder = StatsBuilder::get();
        builder.add_to_root(this);
    }
}

Statable::Statable(stringbuf &str, bool is_root)
    : name(str)
{
    parent = NULL;
    summarize = false;
    default_stats = NULL;
    dump_disabled = false;
    periodic_enabled = false;

    StatsBuilder &builder = StatsBuilder::get();

    if(!is_root)
        builder.add_to_root(this);
}

Statable::Statable(const char *name, Statable *parent)
    : parent(parent)
{
    this->name = name;
    summarize = false;
    dump_disabled = false;
    periodic_enabled = false;

    if(parent) {
        parent->add_child_node(this);
        default_stats = parent->default_stats;
    } else {
        (StatsBuilder::get()).add_to_root(this);
    }
}

Statable::Statable(stringbuf &str, Statable *parent)
    : parent(parent), name(str)
{
    dump_disabled = false;
    periodic_enabled = false;

    if(parent) {
        parent->add_child_node(this);
        default_stats = parent->default_stats;
    } else {
        (StatsBuilder::get()).add_to_root(this);
    }
}

Statable::~Statable()
{
    if(parent) {
        parent->remove_child_node(this);
    }
}

void Statable::set_default_stats(Stats *stats, bool recursive, bool force)
{
    if(default_stats == stats && !force)
        return;

    default_stats = stats;

    // First set stats to leafs
    foreach(i, leafs.count()) {
        leafs[i]->set_default_stats(stats);
    }

    if(!recursive)
        return;

    // Now set stats to all child nodes
    foreach(i, childNodes.count()) {
        childNodes[i]->set_default_stats(stats);
    }
}

ostream& Statable::dump_header(ostream &os) const
{
    if(dump_disabled || !periodic_enabled) return os;

    // First print all the leafs
    foreach(i, leafs.count()) {
        leafs[i]->dump_header(os);
    }
    // Now print all the child nodes
    foreach(i, childNodes.count()) {
        childNodes[i]->dump_header(os);
    }

    return os;
}

ostream& Statable::dump_periodic(ostream &os, Stats *stats) const
{
    if(dump_disabled || !periodic_enabled) return os;

    // First print all the leafs
    foreach(i, leafs.count()) {
        leafs[i]->dump_periodic(os, stats);
    }

    // Now print all the child nodes
    foreach(i, childNodes.count()) {
        childNodes[i]->dump_periodic(os, stats);
    }

    return os;
}

ostream& Statable::dump_summary(ostream &os, Stats *stats, const char* pfx) const
{
    if (dump_disabled || !summarize) return os;

    foreach (i, leafs.count()) {
        leafs[i]->dump_summary(os, stats, pfx);
    }

    foreach (i, childNodes.count()) {
        childNodes[i]->dump_summary(os, stats, pfx);
    }

    return os;
}

ostream& Statable::dump(ostream &os, Stats *stats)
{
    if(dump_disabled) return os;

    os << name << "\t# Node\n";

    // First print all the leafs
    foreach(i, leafs.count()) {
        leafs[i]->dump(os, stats);
    }

    // Now print all the child nodes
    foreach(i, childNodes.count()) {
        childNodes[i]->dump(os, stats);
    }

    return os;
}

YAML::Emitter& Statable::dump(YAML::Emitter &out, Stats *stats)
{
    if(dump_disabled) return out;

    if(name.size()) {
        out << YAML::Key << (char *)name;
        out << YAML::Value;
    }

    // All leafs are key:value maps
    out << YAML::BeginMap;

    // First print all the leafs
    foreach(i, leafs.count()) {
        leafs[i]->dump(out, stats);
    }

    // Now print all the child nodes
    foreach(i, childNodes.count()) {
        childNodes[i]->dump(out, stats);
    }

    out << YAML::EndMap;

    return out;
}

bson_buffer* Statable::dump(bson_buffer *bb, Stats *stats)
{
    if(dump_disabled) return bb;

    bson_buffer *obj = bb;

    if(name.size()) {
        obj = bson_append_start_object(bb, (char *)name);
    }

    // First print all the leafs
    foreach(i, leafs.count()) {
        leafs[i]->dump(obj, stats);
    }

    // Now print all the child nodes
    foreach(i, childNodes.count()) {
        childNodes[i]->dump(obj, stats);
    }

    if(name.size()) {
        bb = bson_append_finish_object(obj);
    }

    return bb;
}

void Statable::add_stats(Stats& dest_stats, Stats& src_stats)
{
    // First add all the leafs
    foreach(i, leafs.count()) {
        leafs[i]->add_stats(dest_stats, src_stats);
    }

    // Now add all the child nodes
    foreach(i, childNodes.count()) {
        childNodes[i]->add_stats(dest_stats, src_stats);
    }
}

void Statable::sub_stats(Stats& dest_stats, Stats& src_stats)
{
    // First add all the leafs
    foreach(i, leafs.count()) {
        leafs[i]->sub_stats(dest_stats, src_stats);
    }

    // Now add all the child nodes
    foreach(i, childNodes.count()) {
        childNodes[i]->sub_stats(dest_stats, src_stats);
    }
}

void Statable::add_periodic_stats(Stats& dest_stats, Stats& src_stats)
{
    if(periodic_enabled)
        add_stats(dest_stats, src_stats);
}

void Statable::sub_periodic_stats(Stats& dest_stats, Stats& src_stats)
{
    if(periodic_enabled)
        sub_stats(dest_stats, src_stats);
}

stringbuf *Statable::get_full_stat_string() const
{
    if (parent)
    {
        stringbuf *parent_name = parent->get_full_stat_string();
        if(parent_name->empty())
            (*parent_name) << name;
        else
            (*parent_name) << "." << name;
        return parent_name;
    } else {
        stringbuf *s = new stringbuf();
        *s << name;
        return s;
    }
}

StatsBuilder *StatsBuilder::_builder = NULL;

Stats* StatsBuilder::get_new_stats()
{
    Stats *stats = new Stats();

    return stats;
}

void StatsBuilder::destroy_stats(Stats *stats)
{
    delete stats->mem;
    delete stats;
}

ostream& StatsBuilder::dump_header(ostream &os) const
{
    if (rootNode->is_dump_periodic())
    {
        os << "sim_cycle";
        rootNode->dump_header(os);
        os << "\n";
    }
    return os;
}

void StatsBuilder::init_timer_stats()
{
    if(!periodic_stats) {
        periodic_stats = get_new_stats();
    }

    if(!temp_stats) {
        temp_stats = get_new_stats();
    }

    if(!temp2_stats) {
        temp2_stats = get_new_stats();
    }
}

ostream& StatsBuilder::dump_periodic(ostream& os, W64 cycle) const
{
    /* Here we perform diff of last saved stats and updated user/kernel stats.
     * Addition/Subtraction is done on the operand1 so we keep two temporary
     * stats as copying is faster than addition/subtraction. */
    *temp_stats = *user_stats;
    add_periodic_stats(*temp_stats, *kernel_stats);

    *temp2_stats = *periodic_stats;
    *periodic_stats = *temp_stats;

    sub_periodic_stats(*temp_stats, *temp2_stats);

    if(rootNode->is_dump_periodic()) {
        os << cycle;
        rootNode->dump_periodic(os, temp_stats);
        os << "\n";
    }

    return os;
}

ostream& StatsBuilder::dump_summary(ostream& os) const
{
    if (rootNode->is_summarize_enabled()) {

        /* First dump the user stats */
        rootNode->dump_summary(os, user_stats, "user");

        /* Now kernel stats */
        rootNode->dump_summary(os, kernel_stats, "kernel");

        /* Total stats at the end */
        rootNode->dump_summary(os, global_stats, "total");
    }

    return os;
}

ostream& StatsBuilder::dump(Stats *stats, ostream &os) const
{
    // First set the stats as default stats in each node
    rootNode->set_default_stats(stats);

    // Now print the stats into ostream
    rootNode->dump(os, stats);

    return os;
}

YAML::Emitter& StatsBuilder::dump(Stats *stats, YAML::Emitter &out) const
{
    // First set the stats as default stats in each node
    rootNode->set_default_stats(stats, true, true);

    // Now print the stats into ostream
    rootNode->dump(out, stats);

    return out;
}

bson_buffer* StatsBuilder::dump(Stats *stats, bson_buffer *bb) const
{
    return rootNode->dump(bb, stats);
}

void StatObjBase::set_default_stats(Stats *stats)
{
    default_stats = stats;
}

