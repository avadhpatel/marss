
#include "statsBuilder.h"

Statable::Statable(const char *name, bool is_root)
{
    this->name = name;
    parent = NULL;
    default_stats = NULL;

    StatsBuilder &builder = StatsBuilder::get();

    if(!is_root)
        builder.add_to_root(this);
}

Statable::Statable(stringbuf &str, bool is_root)
    : name(str)
{
    parent = NULL;
    default_stats = NULL;

    StatsBuilder &builder = StatsBuilder::get();

    if(!is_root)
        builder.add_to_root(this);
}

Statable::Statable(const char *name, Statable *parent)
    : parent(parent)
{
    this->name = name;

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
    if(parent) {
        parent->add_child_node(this);
        default_stats = parent->default_stats;
    } else {
        (StatsBuilder::get()).add_to_root(this);
    }
}

void Statable::set_default_stats(Stats *stats)
{
    default_stats = stats;

    // First set stats to leafs
    foreach(i, leafs.count()) {
        leafs[i]->set_default_stats(stats);
    }

    // Now set stats to all child nodes
    foreach(i, childNodes.count()) {
        childNodes[i]->set_default_stats(stats);
    }
}

ostream& Statable::dump(ostream &os, Stats *stats)
{
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

StatsBuilder StatsBuilder::_builder;

Stats* StatsBuilder::get_new_stats()
{
    Stats *stats = new Stats();

    stats->mem = new W8[STATS_SIZE];
    memset(stats->mem, 0, sizeof(W8) * STATS_SIZE);

    return stats;
}

void StatsBuilder::destroy_stats(Stats *stats)
{
    delete stats->mem;
    delete stats;
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
    rootNode->set_default_stats(stats);

    // Now print the stats into ostream
    rootNode->dump(out, stats);

    return out;
}

void StatObjBase::set_default_stats(Stats *stats)
{
    default_stats = stats;
}
