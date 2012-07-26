
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * This code is released under GPL.
 *
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#ifndef STATS_BUILDER_H
#define STATS_BUILDER_H

#include <globals.h>
#include <superstl.h>

#include <yaml/yaml.h>
#include <bson/bson.h>

#ifdef ENABLE_TESTS
#  define STATS_SIZE 1024*1024*10
#else
#  define STATS_SIZE 1024*1024
#endif

class StatObjBase;
class Stats;

inline static YAML::Emitter& operator << (YAML::Emitter& out, const W64 value)
{
    stringbuf buf;
    buf << value;
    out << buf;
    return out;
}

/**
 * @brief Base class for all classes that has Stats counters
 *
 * To add stats to global statsistics mechanism, inherit this class.
 */
class Statable {
    private:
        dynarray<Statable*> childNodes;
        dynarray<StatObjBase*> leafs;
        bool summarize;
        bool dump_disabled;
        bool periodic_enabled;
        Statable *parent;
        stringbuf name;

        Stats *default_stats;

    public:
        /**
         * @brief Constructor for Statable without any parent
         *
         * @param name Name of this Statable class used in YAML key
         */
        Statable(const char *name);

        /**
         * @brief Constructor for Statable without any parent
         *
         * @param name Name of the Statable class, used in YAML key
         * @param is_root Don't use it, only for internal purpose
         */
        Statable(const char *name, bool is_root);

        /**
         * @brief Constructor for Statable without any parent
         *
         * @param name Name of the Statable class, used in YAML key
         * @param is_root Don't use it, only for internal purpose
         */
        Statable(stringbuf &str, bool is_root);

        /**
         * @brief Contructor for Statable with parent
         *
         * @param name Name of the Statable class, used in YAML key
         * @param parent Parent Statable object
         *
         * By providing parent class, we build a hierarhcy of Statable objects
         * and use it to print hierarhical Statistics.
         */
        Statable(const char *name, Statable *parent);

        /**
         * @brief Constructor for Statable with parent
         *
         * @param str Name of the Statable class, used in YAML Key
         * @param parent Parent Statable object
         */
        Statable(stringbuf &str, Statable *parent);

        /**
         * @brief Default destructor for Statable
         *
         * This makes sure that if 'parent' is present then it removes self
         * from parent's list
         */
        virtual ~Statable();

        /**
         * @brief Add a child Statable node into this object
         *
         * @param child Child Statable object to add
         */
        void add_child_node(Statable *child)
        {
            childNodes.push(child);
        }

        /**
         * @brief Remove given child node from tree
         *
         * @param child Child node to be removed
         */
        void remove_child_node(Statable *child)
        {
            childNodes.remove(child);
        }

        void set_parent(Statable* p)
        {
            parent = p;
        }

        /**
         * @brief Add a child StatObjBase node into this object
         *
         * @param edge A StatObjBase which contains information of Stats
         */
        void add_leaf(StatObjBase *edge)
        {
            leafs.push(edge);
        }

        /**
         * @brief Update the name of this Stats Object
         *
         * @param str String containing new name
         */
        void update_name(const char* str)
        {
            name = str;
        }

		/**
		 * @brief Get the name of Stats Object
		 *
		 * @return name of object
		 */
		char* get_name() const
		{
			return name.buf;
		}

        /**
         * @brief get default Stats*
         *
         * @return
         */
        Stats* get_default_stats()
        {
            return default_stats;
        }

        /**
         * @brief Set default Stats* for this Statable and all its Child
         *
         * @param stats
         */
        void set_default_stats(Stats *stats, bool recursive=true,
                bool force=false);

        /**
         * @brief Disable dumping this Stats node and its child
         */
        void disable_dump() { dump_disabled = true;  }

        /**
         * @brief Enable dumping this Stats node and its child
         */
        void enable_dump()  { dump_disabled = false; }

        void enable_periodic_dump()
        {
            periodic_enabled = true;
            if(parent) parent->enable_periodic_dump();
        }

        void disable_dump_periodic() { periodic_enabled = true; }
        bool is_dump_periodic() { return periodic_enabled; }

        void enable_summary()
        {
            summarize = true;
            if (parent) parent->enable_summary();
        }

        ostream& dump_summary(ostream &os, Stats *stats, const char *pfx) const;
        bool is_summarize_enabled() { return summarize; }

        /**
         * @brief Dump string representation of  Statable and it childs
         *
         * @param os
         * @param stats Stats object from which get stats data
		 * @param pfx Prefix string to add before each node
         *
         * @return
         */
        ostream& dump(ostream &os, Stats *stats, const char* pfx="");

        /**
         * @brief Dump YAML representation of Statable and its childs
         *
         * @param out
         * @param stats Stats object from which get stats data
         *
         * @return
         */
        YAML::Emitter& dump(YAML::Emitter &out, Stats *stats);

        /**
         * @brief Dump BSON representation to Stats
         *
         * @param bb BSON buffer
         * @param stats Stats database to read data from
         *
         * @return updated BSON buffer
         */
        bson_buffer* dump(bson_buffer *bb, Stats *stats);

        void add_stats(Stats& dest_stats, Stats& src_stats);
        void sub_stats(Stats& dest_stats, Stats& src_stats);

        void add_periodic_stats(Stats& dest_stats, Stats& src_stats);
        void sub_periodic_stats(Stats& dest_stats, Stats& src_stats);

        ostream& dump_periodic(ostream &os, Stats *stats) const;

        ostream& dump_header(ostream &os) const;

        stringbuf *get_full_stat_string() const;

		StatObjBase* get_stat_obj(dynarray<stringbuf*> &names, int idx);
};

/**
 * @brief Builder interface to for Stats object
 *
 * This class provides interface to build a Stats object and also provides and
 * interface to map StatObjBase objects to Stats memory.
 *
 * This is a 'singleton' class, so get the object using 'get()' function.
 */
class StatsBuilder {
    private:
        static StatsBuilder *_builder;
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
        /**
         * @brief Get the only StatsBuilder object reference
         *
         * @return
         */
        static StatsBuilder& get()
        {
            if (!_builder)
                _builder = new StatsBuilder();
            return *_builder;
        }

        /**
         * @brief Add a Statble to the root of the Stats tree
         *
         * @param statable
         */
        void add_to_root(Statable *statable)
        {
            assert(statable);
            assert(rootNode);
            rootNode->add_child_node(statable);
            statable->set_parent(rootNode);
        }

        /**
         * @brief Get the offset for given StatObjBase class
         *
         * @param size Size of the memory to be allocted
         *
         * @return Offset value
         */
        W64 get_offset(int size)
        {
            W64 ret_val = stat_offset;
            stat_offset += size;
            assert(stat_offset < STATS_SIZE);
            return ret_val;
        }

        /**
         * @brief Get a new Stats object
         *
         * @return new Stats*
         */
        Stats* get_new_stats();

        /**
         * @brief Delte Stats object
         *
         * @param stats
         */
        void destroy_stats(Stats *stats);

        /**
         * @brief Dump whole Stats tree to ostream
         *
         * @param stats Use given Stats* for values
         * @param os ostream object to dump string
		 * @param pfx Prefix string to add before each node
         *
         * @return
         */
        ostream& dump(Stats *stats, ostream &os, const char *pfx="") const;

        /**
         * @brief Dump Stats tree in YAML format
         *
         * @param stats Use given Stats* for values
         * @param out YAML::Emitter object to dump YAML represetation
         *
         * @return
         */
        YAML::Emitter& dump(Stats *stats, YAML::Emitter &out) const;

        /**
         * @brief Dump Stats tree in BSON format
         *
         * @param stats Stats database
         * @param bb BSON buffer to dump into
         *
         * @return updated BSON buffer
         */
        bson_buffer* dump(Stats *stats, bson_buffer *bb) const;

        void init_timer_stats();

        void add_stats(Stats& dest_stats, Stats& src_stats) const
        {
            rootNode->add_stats(dest_stats, src_stats);
        }

        void sub_stats(Stats& dest_stats, Stats& src_stats) const
        {
            rootNode->sub_stats(dest_stats, src_stats);
        }

        void add_periodic_stats(Stats& dest_stats, Stats& src_stats) const
        {
            if(rootNode->is_dump_periodic())
                add_stats(dest_stats, src_stats);
        }

        void sub_periodic_stats(Stats& dest_stats, Stats& src_stats) const
        {
            if(rootNode->is_dump_periodic())
                sub_stats(dest_stats, src_stats);
        }

        bool is_dump_periodic() { return rootNode->is_dump_periodic(); }
        ostream& dump_header(ostream &os) const;
        ostream& dump_periodic(ostream &os, W64 cycle) const;
        ostream& dump_summary(ostream &os) const;

        void delete_nodes()
        {
            delete rootNode;

            rootNode = new Statable("", true);
            stat_offset = 0;
        }

		StatObjBase* get_stat_obj(stringbuf &name);
		StatObjBase* get_stat_obj(const char *name);
};

/**
 * @brief Statistics class that stores all the statistics counters
 *
 * Stats basically contains a fix memory which is used by all StatObjBase
 * classes to store their variables. Users are not allowed to directly create
 * an object of Stats, they must used StatsBuilder::get_new_stats() function
 * to get one.
 */
class Stats {
    private:
        W8 *mem;

        Stats()
        {
            mem = new W8[STATS_SIZE];
            reset();
        }

    public:
        friend class StatsBuilder;

        W64 base()
        {
            return (W64)mem;
        }

        void reset()
        {
            memset(mem, 0, sizeof(W8) * STATS_SIZE);
        }

        Stats& operator+=(Stats& rhs_stats)
        {
            (StatsBuilder::get()).add_stats(*this, rhs_stats);
            return *this;
        }

        Stats& operator=(Stats& rhs_stats)
        {
            memcpy(mem, rhs_stats.mem, sizeof(W8) * STATS_SIZE);
            return *this;
        }
};

/**
 * @brief Base class for all Statistics container classes
 */
class StatObjBase {
    protected:
        Stats *default_stats;
        Statable *parent;
        stringbuf name;
        bool summarize;
        bool dump_disabled;
        bool periodic_enabled;

    public:
        StatObjBase(const char *name, Statable *parent)
            : parent(parent)
              , summarize(false)
              , dump_disabled(false)
              , periodic_enabled(false)
        {
            this->name = name;
            default_stats = parent->get_default_stats();
            parent->add_leaf(this);
        }

        virtual void set_default_stats(Stats *stats);


        virtual ostream& dump(ostream& os, Stats *stats,
				const char* pfx="") const = 0;
        virtual YAML::Emitter& dump(YAML::Emitter& out,
                Stats *stats) const = 0;
        virtual bson_buffer* dump(bson_buffer* out,
                Stats *stats) const = 0;

        virtual ostream& dump_periodic(ostream &os, Stats *stats) const = 0;

        void disable_dump_periodic()
        {
            periodic_enabled = false;
            /* NOTE: We don't disable parent's because other child
             * might have enabled periodic dump */
        }

        void enable_periodic_dump()
        {
            periodic_enabled = true;
            parent->enable_periodic_dump();
        }

        inline bool is_dump_periodic() const { return periodic_enabled; }

        void enable_summary()
        {
            summarize = true;
            parent->enable_summary();
        }

        inline bool is_summarize_enabled() const { return summarize; }

        stringbuf *get_full_stat_string() const
        {
            if (parent){
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

		/**
		 * @brief Get name of the Stats Object
		 *
		 * @return char* containing name
		 */
		char* get_name() const
		{
			return name.buf;
		}

        virtual ostream &dump_header(ostream &os) {
            if (is_dump_periodic()) {
                stringbuf *full_string = get_full_stat_string();
                os << ","<< (*full_string);

                delete full_string;
            }
            return os;
        }

        virtual ostream& dump_summary(ostream& os, Stats* stats, const char* pfx) const = 0;

        virtual void add_stats(Stats& dest_stats, Stats& src_stats) = 0;
        virtual void sub_stats(Stats& dest_stats, Stats& src_stats) = 0;

        virtual void add_periodic_stats(Stats& dest_stats, Stats& src_stats) = 0;
        virtual void sub_periodic_stats(Stats& dest_stats, Stats& src_stats) = 0;

        void disable_dump() { dump_disabled = true; }
        void enable_dump() { dump_disabled = false; }
        bool is_dump_disabled() const { return dump_disabled; }
};

/**
 * @brief Create a Stat object of type T
 *
 * This class povides an easy interface to create basic statistics counters for
 * the simulator. It also provides easy way to generate YAML and BSON
 * representation of the the object for final Stats dump.
 */
template<typename T>
class StatObj : public StatObjBase {
    private:
        W64 offset;

        T *default_var;

        inline void set_default_var_ptr()
        {
            if(default_stats) {
                default_var = (T*)(default_stats->base() + offset);
            } else {
                default_var = NULL;
            }
        }

    public:
        /**
         * @brief Default constructor for StatObj
         *
         * @param name Name of that StatObj used in YAML Key map
         * @param parent Statable* which contains this stats object
         */
        StatObj(const char *name, Statable *parent)
            : StatObjBase(name, parent)
        {
            StatsBuilder &builder = StatsBuilder::get();

            offset = builder.get_offset(sizeof(T));

            set_default_var_ptr();
        }

        /**
         * @brief Set the default Stats*
         *
         * @param stats A pointer to Stats structure
         *
         * This function will set the default Stats* to use for all future
         * operations like ++, += etc. untill its changed.
         */
        void set_default_stats(Stats *stats)
        {
            StatObjBase::set_default_stats(stats);
            set_default_var_ptr();
        }

        /**
         * @brief ++ operator - like a++
         *
         * @param dummy
         *
         * @return object of type T with updated value
         */
        inline T operator++(int dummy)
        {
            assert(default_var);
            T ret = (*default_var)++;
            return ret;
        }

        /**
         * @brief ++ operator - like ++a
         *
         * @return object of type T with updated value
         */
        inline T operator++()
        {
            assert(default_var);
            (*default_var)++;
            return (*default_var);
        }

        /**
         * @brief -- opeartor - like a--
         *
         * @param dummy
         *
         * @return object of type T with update value
         */
        inline T operator--(int dummy) {
            assert(default_var);
            T ret = (*default_var)--;
            return ret;
        }

        /**
         * @brief -- opeartor - like --a
         *
         * @return object of type T with update value
         */
        inline T operator--() {
            assert(default_var);
            (*default_var)--;
            return (*default_var);
        }

        /**
         * @brief -= operator
         *
         * @param val Amount to decrent
         *
         * @return T& with updated value
         */
        inline T& operator -= (T& val) {
            (*default_var) -= val;
            return (*default_var);
        }

        inline T& operator=(T& val) {
            assert(default_var);
            (*default_var) = val;
            return (*default_var);
        }

        /**
         * @brief + operator
         *
         * @param b value to be added
         *
         * @return object of type T with new value
         */
        inline T operator +(const T &b) const {
            assert(default_var);
            T ret = (*default_var) + b;
            return ret;
        }

        /**
         * @brief + operator with StatObj
         *
         * @param statObj
         *
         * @return object of type T with new value
         */
        inline T operator +(const StatObj<T> &statObj) const {
            assert(default_var);
            assert(statObj.default_var);
            T ret = (*default_var) + (*statObj.default_var);
            return ret;
        }

        /**
         * @brief += operator
         *
         * @param b value to be added
         *
         * @return object of type T with new value
         */
        inline T operator +=(const T &b) const {
            assert(default_var);
            *default_var += b;
            return *default_var;;
        }

        /**
         * @brief += operator with StatObj
         *
         * @param statObj
         *
         * @return object of type T with new value
         */
        inline T operator +=(const StatObj<T> &statObj) const {
            assert(default_var);
            assert(statObj.default_var);
            *default_var += (*statObj.default_var);
            return  *default_var;
        }

        /**
         * @brief - operator
         *
         * @param b value to remove
         *
         * @return object of type T with new value
         */
        inline T operator -(const T &b) const {
            assert(default_var);
            T ret = (*default_var) - b;
            return ret;
        }

        /**
         * @brief - operator with StatObj
         *
         * @param statObj
         *
         * @return object of type T with new value
         */
        inline T operator -(const StatObj<T> &statObj) const {
            assert(default_var);
            assert(statObj.default_var);
            T ret = (*default_var) - (*statObj.default_var);
            return ret;
        }

        /**
         * @brief * operator
         *
         * @param b value to multiply
         *
         * @return object of type T with new value
         */
        inline T operator *(const T &b) const {
            assert(default_var);
            T ret = (*default_var) * b;
            return ret;
        }

        /**
         * @brief * operator with StatObj
         *
         * @param statObj
         *
         * @return object of type T with new value
         */
        inline T operator *(const StatObj<T> &statObj) const {
            assert(default_var);
            assert(statObj.default_var);
            T ret = (*default_var) * (*statObj.default_var);
            return ret;
        }

        /**
         * @brief / operator
         *
         * @param b value to divide
         *
         * @return object of type T with new value
         */
        inline T operator /(const T &b) const {
            assert(default_var);
            T ret = (*default_var) / b;
            return ret;
        }

        /**
         * @brief / operator with StatObj
         *
         * @param statObj
         *
         * @return object of type T with new value
         */
        inline T operator /(const StatObj<T> &statObj) const {
            assert(default_var);
            assert(statObj.default_var);
            T ret = (*default_var) / (*statObj.default_var);
            return ret;
        }

        /**
         * @brief () operator to use given Stats* instead of default
         *
         * @param stats Stats* to use instead of default Stats*
         *
         * @return reference of type T in given Stats
         *
         * This function can be used when you don't want to operate on the
         * default stats of the object and specify other Stats* without
         * changing the default_stats in the object.
         *
         * For example, use this function like below:
         *      cache.read.hit(kernel_stats)++
         * This will increase the 'cache.read.hit' counter in kerenel_stats
         * instead of 'default_stats' of 'cache.read.hit'.
         */
        inline T& operator()(Stats *stats) const
        {
            return *(T*)(stats->base() + offset);
        }

        /**
         * @brief Dump a string representation to ostream
         *
         * @param os ostream& where string will be added
         * @param stats Stats object from which get stats data
		 * @param pfx Prefix string to add before printing stats name
         *
         * @return updated ostream object
         */
        ostream& dump(ostream& os, Stats *stats, const char* pfx="") const
        {
            if(is_dump_disabled()) return os;

            T var = (*this)(stats);
			stringbuf *full_string = get_full_stat_string();

            os << pfx << *full_string << ":" << var << "\n";

			delete full_string;
            return os;
        }

        /**
         * @brief Dump YAML representation of this object
         *
         * @param out YAML::Emitter object
         * @param stats Stats object from which get stats data
         *
         * @return
         */
        YAML::Emitter& dump(YAML::Emitter &out, Stats *stats) const
        {
            if(is_dump_disabled()) return out;

            T var = (*this)(stats);

            out << YAML::Key << (char *)name;
            out << YAML::Value << var;

            return out;
        }

        /**
         * @brief Dump StatObj to BSON format
         *
         * @param bb BSON buffer to dump into
         * @param stats Stats database to read string from
         *
         * @return updated BSON buffer
         */
        bson_buffer* dump(bson_buffer *bb, Stats *stats) const
        {
            if(is_dump_disabled()) return bb;

            T var = (*this)(stats);

            // FIXME : Currently we dump all values as 'long'
            return bson_append_long(bb, (char *)name, var);
        }

        void add_stats(Stats& dest_stats, Stats& src_stats)
        {
            T& dest_var = (*this)(&dest_stats);
            dest_var += (*this)(&src_stats);
        }

        void sub_stats(Stats& dest_stats, Stats& src_stats)
        {
            T& dest_var = (*this)(&dest_stats);
            dest_var -= (*this)(&src_stats);
        }

        void add_periodic_stats(Stats& dest_stats, Stats& src_stats)
        {
            if(is_dump_periodic()) {
                add_stats(dest_stats, src_stats);
            }
        }

        void sub_periodic_stats(Stats& dest_stats, Stats& src_stats)
        {
            if(is_dump_periodic()) {
                sub_stats(dest_stats, src_stats);
            }
        }

        ostream &dump_periodic(ostream &os, Stats *stats) const
        {
            if (is_dump_periodic())
            {
                T& val = (*this)(stats);
                os << "," << val;
            }
            return os;
        }

        ostream &dump_summary(ostream &os, Stats *stats, const char* pfx) const
        {
            if (is_summarize_enabled()) {
                T& val = (*this)(stats);
                stringbuf *name = get_full_stat_string();
                os << pfx << "." << (*name) << " = " << val << endl;
            }

            return os;
        }
};

/**
 * @brief Create a Stat object which is array of T[size]
 *
 * This class provides easy interface to create an array of statistic counters
 */
template<typename T, int size>
class StatArray : public StatObjBase {

    private:
        W64 offset;
        T* default_var;
        const char** labels;
        bitvec<size> periodic_flag;
        bitvec<size> summarize_flag;

        inline void set_default_var_ptr()
        {
            if(default_stats) {
                default_var = (T*)(default_stats->base() + offset);
            } else {
                default_var = NULL;
            }
        }

    public:

        typedef T BaseArr[size];

        /**
         * @brief Default constructor
         *
         * @param name Name of the stat array
         * @param parent Parent Statable object of this
         */
        StatArray(const char *name, Statable *parent, const char** labels_=NULL)
            : StatObjBase(name, parent)
              , labels(labels_)
        {
            StatsBuilder &builder = StatsBuilder::get();

            offset = builder.get_offset(sizeof(T) * size);

            set_default_var_ptr();
        }

        /**
         * @brief Set the default Stats*
         *
         * @param stats A pointer to Stats structure
         *
         * This function will set the default Stats* to use for all future
         * operations like ++, += etc. untill its changed.
         */
        void set_default_stats(Stats *stats)
        {
            StatObjBase::set_default_stats(stats);
            set_default_var_ptr();
            assert(default_var);
        }

        /**
         * @brief [] operator to access one element of array
         *
         * @param index index of the element to access from array
         *
         * @return reference to the requested element
         */
        inline T& operator[](const int index)
        {
            assert(index < size);
            assert(default_var);

            BaseArr& arr = *(BaseArr*)(default_var);
            return arr[index];
        }

        /**
         * @brief () operator to use given Stats* instead of default
         *
         * @param stats Stats* to use instead of default Stats*
         *
         * @return reference of array of type T in given Stats
         *
         */
        BaseArr& operator()(Stats *stats) const
        {
            return *(BaseArr*)(stats->base() + offset);
        }

        /**
         * @brief dump string representation of StatArray
         *
         * @param os dump into this ostream object
         * @param stats Stats* from which to get array values
		 * @param pfx Prefix string to add before printing stats name
         *
         * @return
         */
        ostream& dump(ostream &os, Stats *stats, const char* pfx="") const
        {
            if(is_dump_disabled()) return os;

			stringbuf *full_string = get_full_stat_string();

			if (labels) {
				BaseArr& arr = (*this)(stats);
				foreach(i, size) {
					os << pfx << *full_string << "." << labels[i] <<
						":" << arr[i] << "\n";
				}
			} else {
				os << pfx << *full_string << ":";
				BaseArr& arr = (*this)(stats);
				foreach(i, size) {
					os << arr[i] << " ";
				}
				os << "\n";
			}

			delete full_string;

            return os;
        }

        /**
         * @brief dump YAML representation of StatArray
         *
         * @param out dump into this object
         * @param stats Stats* from which to get array values
         *
         * @return
         */
        YAML::Emitter& dump(YAML::Emitter &out, Stats *stats) const
        {
            if(is_dump_disabled()) return out;

            out << YAML::Key << (char *)name;
            out << YAML::Value;

            if(labels) {
                out << YAML::BeginMap;

                BaseArr& arr = (*this)(stats);
                foreach(i, size) {
                    out << YAML::Key << labels[i];
                    out << YAML::Value << arr[i];
                }

                out << YAML::EndMap;
            } else {
                out << YAML::Flow;
                out << YAML::BeginSeq;

                BaseArr& arr = (*this)(stats);
                foreach(i, size) {
                    out << arr[i];
                }

                out << YAML::EndSeq;
                out << YAML::Block;
            }

            return out;
        }

        /**
         * @brief Dump StatArray to BSON format
         *
         * @param bb BSON buffer to dump into
         * @param stats Stats database to read string from
         *
         * @return updated BSON buffer
         */
        bson_buffer* dump(bson_buffer *bb, Stats *stats) const
        {
            if(is_dump_disabled()) return bb;

            char numstr[16];
            bson_buffer *arr;

            BaseArr& val = (*this)(stats);
            if(labels) {
                arr = bson_append_start_object(bb, (char *)name);

                foreach(i, size) {
                    bson_append_long(arr, labels[i], val[i]);
                }
            } else {
                arr = bson_append_start_array(bb, (char *)name);

                foreach(i, size) {
                    bson_numstr(numstr, i);
                    bson_append_long(arr, numstr, val[i]);
                }
            }

            return bson_append_finish_object(arr);
        }

        void add_stats(Stats& dest_stats, Stats& src_stats)
        {
            BaseArr& dest_arr = (*this)(&dest_stats);
            BaseArr& src_arr = (*this)(&src_stats);
            foreach(i, size) {
                dest_arr[i] += src_arr[i];
            }
        }

        void sub_stats(Stats& dest_stats, Stats& src_stats)
        {
            BaseArr& dest_arr = (*this)(&dest_stats);
            BaseArr& src_arr = (*this)(&src_stats);
            foreach(i, size) {
                dest_arr[i] -= src_arr[i];
            }
        }

        void add_periodic_stats(Stats& dest_stats, Stats& src_stats)
        {
            if(is_dump_periodic()) {
                add_stats(dest_stats, src_stats);
            }
        }

        void sub_periodic_stats(Stats& dest_stats, Stats& src_stats)
        {
            if(is_dump_periodic()) {
                sub_stats(dest_stats, src_stats);
            }
        }

        /**
         * @brief Enable periodic dump of this StatsArray
         *
         * @param id If given, enables only selected element from array
         */
        void enable_periodic_dump(int id = -1)
        {
            StatObjBase::enable_periodic_dump();
            if(id == -1) {
                periodic_flag.setall();
            } else {
                assert(id < size);
                periodic_flag[id] = 1;
            }
        }

        ostream &dump_header(ostream &os)
        {
            if (!is_dump_periodic()) return os;

            stringbuf *full_string = get_full_stat_string();

            foreach(i, size) {
                if(periodic_flag[i]) {
                    os << "," << *full_string;

                    if(labels) {
                        os << "." << labels[i];
                    } else {
                        os << "." << i;
                    }
                }
            }

            delete full_string;
            return os;
        }

        ostream &dump_periodic(ostream &os, Stats *stats) const
        {
            if (!is_dump_periodic()) return os;

            BaseArr& arr = (*this)(stats);

            foreach(i, size) {
                if(periodic_flag[i]) {
                    os << "," << arr[i];
                }
            }

            return os;
        }

        void enable_summary(int id = -1)
        {
            StatObjBase::enable_summary();
            if(id == -1) {
                summarize_flag.setall();
            } else {
                assert(id < size);
                summarize_flag[id] = 1;
            }
        }

        ostream &dump_summary(ostream &os, Stats *stats, const char* pfx) const
        {
            if (!is_summarize_enabled()) return os;

            BaseArr& arr = (*this)(stats);
            stringbuf* name = get_full_stat_string();

            foreach (i, size) {
                if(summarize_flag[i]) {
                    os << pfx << "." << (*name);

                    if (labels) {
                        os << "." << labels[i];
                    } else {
                        os << "." << i;
                    }

                    os << " = " << arr[i] << endl;
                }
            }

            return os;
        }

        /**
         * @brief Get size of this array
         *
         * @return size
         */
        int length() const
        {
            return size;
        }
};

/**
 * @brief Create a Stat string object
 *
 * This class provides an interface to create Strings in a Stats database. This
 * string object is mainly provided to add tags and other informations to the
 * Stats database. StatString doesn't provide 'add_stats' support so if you
 * want to add two StatStrings object then do it manually.
 *
 * Maximum length of this string is 256 bytes.
 */
class StatString : public StatObjBase {

    private:
        W64 offset;
        char* default_var;
        char split[8];

        inline void set_default_var_ptr()
        {
            if(default_stats) {
                default_var = (char*)(default_stats->base() + offset);
            } else {
                default_var = NULL;
            }
        }

    public:

        static const uint16_t MAX_STAT_STR_SIZE = 256;

        /**
         * @brief Default constructor
         *
         * @param name Name of the stats object
         * @param parent Parent Statable object of this
         */
        StatString(const char *name, Statable *parent)
            : StatObjBase(name, parent)
        {
            split[0] = '\0';

            StatsBuilder& builder = StatsBuilder::get();

            offset = builder.get_offset(sizeof(char) * MAX_STAT_STR_SIZE);

            set_default_var_ptr();
        }

        /**
         * @brief Set string split string
         *
         * @param split_val string value to be used as splitter
         *
         * When you specify splitter string, the string will be automatically
         * split into an array and that array will be dumped.
         */
        void set_split(const char *split_val)
        {
            strcpy(split, split_val);
        }

        /**
         * @brief Set the default Stats*
         *
         * @param stats A pointer to Stats structure
         */
        void set_default_stats(Stats *stats)
        {
            StatObjBase::set_default_stats(stats);
            set_default_var_ptr();
            assert(default_var);
        }

        /**
         * @brief Copy string from given char *
         *
         * @param str A char* string to copy from
         */
        inline char* operator= (const char* str)
        {
            if(strlen(str) >= MAX_STAT_STR_SIZE) {
                assert(0);
            }

            assert(default_var);

            strcpy(default_var, str);

            return default_var;
        }

        /**
         * @brief Copy string from given stringbuf
         *
         * @param str A stringbuf object to copy from
         */
        inline char* operator= (const stringbuf& str)
        {
            if(str.size() >= MAX_STAT_STR_SIZE) {
                assert(0);
            }

            return (*this) = str.buf;
        }

        /**
         * @brief Copy string from given char * to specified Stats
         *
         * @param stats A Stats pointer to specify database
         * @param str A char* string to copy from
         */
        inline void set(Stats *stats, const char* str)
        {
            if(strlen(str) >= MAX_STAT_STR_SIZE) {
                assert(0);
            }

            char* var = (*this)(stats);
            assert(var);

            strcpy(var, str);
        }

        /**
         * @brief Copy string from given stringbuf to specified Stats
         *
         * @param stats A Stats pointer to specify database
         * @param str A stringbuf object to copy from
         */
        inline void set(Stats *stats, const stringbuf& str)
        {
            this->set(stats, str.buf);
        }

        /**
         * @brief Get a char* of given Stats database
         *
         * @param stats A Stats database pointer
         */
        inline char* operator()(Stats *stats) const
        {
            return (char*)(stats->base() + offset);
        }

        /**
         * @brief Dump string of given database to given ostream
         *
         * @param os stream to dump
         * @param stats Stats database to read string from
		 * @param pfx Prefix string to add before printing stats name
         */
        ostream& dump(ostream& os, Stats *stats, const char* pfx="") const
        {
            if(is_dump_disabled()) return os;

            char* var = (*this)(stats);
			stringbuf *full_string = get_full_stat_string();

            if(split[0] != '\0') {
                dynarray<stringbuf*> tags;
                stringbuf st_tags; st_tags << var;
                st_tags.split(tags, split);

                os << pfx << *full_string << "[";
                foreach(i, tags.size()) {
                    os << i << ":" << tags[i]->buf << ", ";
                }
                os << "]\n";
            } else {
                os << pfx << *full_string << ":" << var << "\n";
            }

			delete full_string;

            return os;
        }

        /**
         * @brief Dump string of given database in YAML format
         *
         * @param out dump stream in this YAML Emitter
         * @param stats Stats database to read string from
         */
        YAML::Emitter& dump(YAML::Emitter &out, Stats *stats) const
        {
            if(is_dump_disabled()) return out;

            char* var = (*this)(stats);

            if(split[0] != '\0') {
                dynarray<stringbuf*> tags;
                stringbuf st_tags; st_tags << var;
                st_tags.split(tags, split);

                out << YAML::Key << (char *)name;
                out << YAML::Value;

                out << YAML::Flow;
                out << YAML::BeginSeq;

                foreach(i, tags.size()) {
                    out << tags[i]->buf;
                }

                out << YAML::EndSeq;
                out << YAML::Block;
            } else {
                out << YAML::Key << (char *)name;
                out << YAML::Value << var;
            }

            return out;
        }

        /**
         * @brief Dump StatString to BSON format
         *
         * @param bb BSON buffer to dump into
         * @param stats Stats database to read string from
         *
         * @return updated BSON buffer
         */
        bson_buffer* dump(bson_buffer *bb, Stats *stats) const
        {
            if(is_dump_disabled()) return bb;

            char* var = (*this)(stats);

            if(split[0] != '\0') {
                dynarray<stringbuf*> tags;
                stringbuf st_tags; st_tags << var;
                st_tags.split(tags, split);

                char numstr[16];
                bson_buffer *arr = bson_append_start_array(bb, (char *)name);

                foreach(i, tags.size()) {
                    bson_numstr(numstr, i);
                    bson_append_string(arr, numstr, tags[i]->buf);
                }

                return bson_append_finish_object(arr);
            }

            return bson_append_string(bb, (char *)name, var);
        }

        void add_stats(Stats& dest_stats, Stats& src_stats)
        {
            /* NOTE: We don't do auto addition os stats string */
        }

        void sub_stats(Stats& dest_stats, Stats& src_stats)
        { }

        void add_periodic_stats(Stats& dest_stats, Stats& src_stats)
        { }

        void sub_periodic_stats(Stats& dest_stats, Stats& src_stats)
        { }

        ostream &dump_periodic(ostream &os, Stats *stats) const
        {
            return os;
        }

        ostream &dump_summary(ostream &os, Stats *stats, const char* pfx) const
        {
            return os;
        }
};

/**
 * @brief Add given stats counters
 *
 * This struct provides computation function to Add stats elements.
 */
struct StatObjFormulaAdd {
    typedef dynarray<StatObj<W64>* > elems_t;

    static W64 compute(Stats* stats, const elems_t& elems)
    {
        W64 ret = 0;

        foreach(i, elems.count()) {
            StatObj<W64>& e = *elems[i];
            ret += e(stats);
        }

        return ret;
    }
};

/**
 * @brief Perform Division of given Stats counters
 */
struct StatObjFormulaDiv {
    typedef dynarray<StatObj<W64>* > elems_t;

    static double compute(Stats* stats, const elems_t& elems)
    {
        double ret = 0;

        assert(elems.count() == 2);
        double val1 = double((*elems[0])(stats));
        double val2 = double((*elems[1])(stats));

        if(val2 == 0)
            return ret;

        ret = val1/val2;

        return ret;
    }
};

/**
 * @brief Statistics Class that supports User specific Formula's
 *
 * @tparam T Type of Stats Objects to perform computation
 * @tparam K Type of result to store
 * @tparam F Formula to compute result
 *
 * The computation is done when any of the 'dump' function is called. This
 * class only supports computation over StatObj<T> type objects.
 */
template<typename T, typename K, typename F>
class StatEquation : public StatObj<K> {
    private:
        typedef StatObj<K> base_t;
        typedef dynarray<StatObj<T>* > elems_t;
        elems_t elems;
        F formula;

        /**
         * @brief Perform computation and store result
         *
         * @param stats Stats Database used for computation
         */
        void compute(Stats* stats) const
        {
            K& val = (*this)(stats);
            val = formula.compute(stats, elems);
        }

    public:

        /**
         * @brief Default Constructor
         *
         * @param name Name of this object
         * @param parent Parent Stat object
         */
        StatEquation(const char *name, Statable *parent)
            : StatObj<K>(name, parent)
        { }

        /**
         * @brief Add StatObj<T> object for computation
         *
         * @param obj element to add to the computation
         */
        void add_elem(StatObj<T>* obj)
        {
            elems.push(obj);
        }

        void enable_periodic_dump()
        {
            base_t::enable_periodic_dump();

            foreach(i, elems.count())
                elems[i]->enable_periodic_dump();
        }

        /**
         * @brief Print value of this Stats object
         *
         * @param os stream to dump into
         * @param stats Stats Database that holds the value
		 * @param pfx Prefix string to add before printing stats name
         *
         * @return Updated stream
         */
        ostream& dump(ostream& os, Stats *stats, const char* pfx="") const
        {
            compute(stats);
            return base_t::dump(os, stats, pfx);
        }

        /**
         * @brief Dump YAML value of this Stats Object
         *
         * @param out YAML stream to dump into
         * @param stats Stats Database that holds the value
         *
         * @return Updated YAML stream
         */
        YAML::Emitter& dump(YAML::Emitter& out,
                Stats *stats) const
        {
            compute(stats);
            return base_t::dump(out, stats);
        }

        /**
         * @brief Dump BSON value of this Stats Object
         *
         * @param out BSON stream to dump into
         * @param stats Stats Database that holds the value
         *
         * @return Update BSON stream
         */
        bson_buffer* dump(bson_buffer* out,
                Stats *stats) const
        {
            compute(stats);
            return base_t::dump(out, stats);
        }

        /**
         * @brief Dump Periodic value of this Stats Object
         *
         * @param os output stream
         *
         * @return Updated output stream
         */
        ostream& dump_periodic(ostream &os, Stats *stats) const
        {
            compute(stats);
            base_t::dump_periodic(os, stats);
            return os;
        }
};

#endif // STATS_BUILDER_H
