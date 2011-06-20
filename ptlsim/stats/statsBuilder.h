
#ifndef STATS_BUILDER_H
#define STATS_BUILDER_H

#include <globals.h>
#include <superstl.h>

#include <yaml/yaml.h>
#include <bson/bson.h>

#define STATS_SIZE 1024*1024

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
        bool dump_disabled;
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
         * @brief Add a child Statable node into this object
         *
         * @param child Child Statable object to add
         */
        void add_child_node(Statable *child)
        {
            childNodes.push(child);
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

        /**
         * @brief Dump string representation of  Statable and it childs
         *
         * @param os
         * @param stats Stats object from which get stats data
         *
         * @return
         */
        ostream& dump(ostream &os, Stats *stats);

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

        ostream& dump_periodic(ostream &os) const;

        ostream& dump_header(ostream &os) const;

        bool is_dump_periodic_disabled() const;

        stringbuf *get_full_stat_string();



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
        /**
         * @brief Get the only StatsBuilder object reference
         *
         * @return
         */
        static StatsBuilder& get()
        {
            return _builder;
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
         *
         * @return
         */
        ostream& dump(Stats *stats, ostream &os) const;

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

        void add_stats(Stats& dest_stats, Stats& src_stats)
        {
            rootNode->add_stats(dest_stats, src_stats);
        }

        ostream& dump_header(ostream &os) const;
        ostream& dump_periodic(ostream &os, W64 cycle) const;

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
            mem = NULL;
        }

    public:
        friend class StatsBuilder;

        W64 base()
        {
            return (W64)mem;
        }

        Stats& operator+=(Stats& rhs_stats)
        {
            (StatsBuilder::get()).add_stats(*this, rhs_stats);
            return *this;
        }
};

/**
 * @brief Base class for all Statistics container classes
 */
class StatObjBase {
    protected:
        Stats *time_stats;
        Stats *default_stats;
        Statable *parent;
        stringbuf name;
        bool dump_disabled;

    public:
        StatObjBase(const char *name, Statable *parent)
            : parent(parent)
              , time_stats(NULL)
              , dump_disabled(false)
        {
            this->name = name;
            default_stats = parent->get_default_stats();
            parent->add_leaf(this);
        }

        virtual void set_default_stats(Stats *stats);
        virtual void set_time_stats(Stats *stats);


        virtual ostream& dump(ostream& os, Stats *stats) const = 0;
        virtual YAML::Emitter& dump(YAML::Emitter& out,
                Stats *stats) const = 0;
        virtual bson_buffer* dump(bson_buffer* out,
                Stats *stats) const = 0;

        virtual ostream& dump_periodic(ostream &os) const = 0;

        inline bool is_dump_periodic_disabled() const { return time_stats == NULL; }

        stringbuf *get_full_stat_string()
        {
            if (parent){
                stringbuf *parent_name = parent->get_full_stat_string();
                (*parent_name) << "." << name;
                return parent_name;
            } else {
                return new stringbuf(name);
            }
        }

        virtual ostream &dump_header(ostream &os) {
            if (!is_dump_periodic_disabled()) {
                stringbuf *full_string = get_full_stat_string();
                os << ","<< (*full_string);
                /* FIXME: this should be deleted, but for some reason libc
                   does not like that one bit, need to figure out why ...
                   */

                delete full_string;
            }
        }


        virtual void add_stats(Stats& dest_stats, Stats& src_stats) = 0;


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
        T *time_var;

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
            , time_var(NULL)
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

        void set_time_stats(Stats *stats)
        {
            StatObjBase::set_time_stats(stats);
            time_var = (T*)(time_stats->base() + offset);
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
            if (time_var)
            {
                (*time_var)++;
            }
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
            if (time_var)
            {
                (*time_var)++;
            }

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
            if (time_var)
            {
                (*time_var)--;
            }
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
            if (time_var)
            {
                (*time_var)--;
            }
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
            if (time_var)
            {
                (*time_var)+= b;
            }
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
            if (time_var && statObj.time_var)
            {
                /*XXX: if, for some reason, the dump_periodic is called at
                  different intervals for the two statObj's, this value is
                  meaningless, however, this is currently not that case
                 */

                (*time_var) += *statObj.time_var;
            }
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
         *
         * @return updated ostream object
         */
        ostream& dump(ostream& os, Stats *stats) const
        {
            if(is_dump_disabled()) return os;

            T var = (*this)(stats);

            os << name << ":" << var << "\n";

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

        ostream &dump_periodic(ostream &os) const
        {
            if (time_var)
            {
                os << "," << (*time_var);
                // reset the time var for the next epoch
                *time_var = 0;
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
         *
         * @return
         */
        ostream& dump(ostream &os, Stats *stats) const
        {
            if(is_dump_disabled()) return os;

            os << name << ": ";
            BaseArr& arr = (*this)(stats);
            foreach(i, size) {
                os << arr[i] << " ";
            }
            os << "\n";

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

        ostream &dump_periodic(ostream &os) const
        {
            return os;
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

        static const int MAX_STAT_STR_SIZE = 256;

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
         */
        ostream& dump(ostream& os, Stats *stats) const
        {
            if(is_dump_disabled()) return os;

            char* var = (*this)(stats);

            if(split[0] != '\0') {
                dynarray<stringbuf*> tags;
                stringbuf st_tags; st_tags << var;
                st_tags.split(tags, split);

                os << name << "[";
                foreach(i, tags.size()) {
                    os << i << ":" << tags[i]->buf << ", ";
                }
                os << "\n";
            } else {
                os << name << ":" << var << "\n";
            }

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

        ostream &dump_periodic(ostream &os) const
        {
            return os;
        }

};

#endif // STATS_BUILDER_H
