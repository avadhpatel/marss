
#ifndef STATS_BUILDER_H
#define STATS_BUILDER_H

#include <globals.h>
#include <superstl.h>

#include <yaml/yaml.h>

#define STATS_SIZE 1024*2

class StatObjBase;
class Stats;

class Statable {
    private:
        dynarray<Statable*> childNodes;
        dynarray<StatObjBase*> leafs;
        Statable *parent;
        stringbuf name;

        Stats *default_stats;

    public:
        Statable(const char *name, bool is_root = false);
        Statable(stringbuf &str, bool is_root = false);
        Statable(const char *name, Statable *parent);
        Statable(stringbuf &str, Statable *parent);

        void add_child_node(Statable *child)
        {
            childNodes.push(child);
        }

        void add_leaf(StatObjBase *edge)
        {
            leafs.push(edge);
        }

        Stats* get_default_stats()
        {
            return default_stats;
        }

        void set_default_stats(Stats *stats);

        ostream& dump(ostream &os);
        YAML::Emitter& dump(YAML::Emitter &os);
};

class StatsBuilder {
    private:
        static StatsBuilder _builder;
        Statable *rootNode;
        W64 stat_offset;

        StatsBuilder()
        {
            rootNode = new Statable("", true);
            stat_offset = 0;
        }

        ~StatsBuilder()
        {
            delete rootNode;
        }

    public:
        static StatsBuilder& get()
        {
            return _builder;
        }

        void add_to_root(Statable *statable)
        {
            assert(statable);
            assert(rootNode);
            rootNode->add_child_node(statable);
        }

        W64 get_offset(int size)
        {
            W64 ret_val = stat_offset;
            stat_offset += size;

            assert(stat_offset < STATS_SIZE);
            return ret_val;
        }

        Stats* get_new_stats();
        void destroy_stats(Stats *stats);

        ostream& dump(Stats *stats, ostream &os) const;
        YAML::Emitter& dump(Stats *stats, YAML::Emitter &out) const;
};

class Stats {
    private:
        W8 *mem;

        Stats()
        {
            mem = NULL;
        }

    public:
        friend class StatsBuilder;

        W64 base()
        {
            return (W64)mem;
        }
};

class StatObjBase {
    protected:
        Stats *default_stats;
        Statable *parent;
        stringbuf name;

    public:
        StatObjBase(const char *name, Statable *parent)
            : parent(parent)
        {
            this->name = name;
            default_stats = parent->get_default_stats();
            parent->add_leaf(this);
        }

        virtual void set_default_stats(Stats *stats);

        virtual ostream& dump(ostream& os) const = 0;
        virtual YAML::Emitter& dump(YAML::Emitter& out) const = 0;
};

template<typename T>
class StatObj : public StatObjBase {
    private:
        W64 offset;

        T *default_var;

    public:
        StatObj(const char *name, Statable *parent)
            : StatObjBase(name, parent)
        {
            StatsBuilder &builder = StatsBuilder::get();

            offset = builder.get_offset(sizeof(T));

            set_default_var_ptr();
        }

        inline void set_default_var_ptr()
        {
            if(default_stats) {
                default_var = (T*)(default_stats->base() + offset);
            } else {
                default_var = NULL;
            }
        }

        void set_default_stats(Stats *stats)
        {
            StatObjBase::set_default_stats(stats);
            set_default_var_ptr();
        }

        inline T operator++(int dummy)
        {
            assert(default_var);
            T ret = (*default_var)++;
            return ret;
        }

        ostream& dump(ostream& os) const
        {
            assert(default_var);
            T var = *default_var;

            os << name << ":" << var << "\n";

            return os;
        }

        YAML::Emitter& dump(YAML::Emitter &out) const
        {
            assert(default_var);
            T var = *default_var;

            out << YAML::Key << (char *)name;

            stringbuf val;
            val << var;

            out << YAML::Value << (char *)val;

            return out;
        }
};

#endif // STATS_BUILDER_H
