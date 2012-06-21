
/*
 * MARSSx86 : A Full System Computer-Architecture Simulator
 *
 * This code is released under GPL.
 *
 * Copyright 2011 Avadh Patel <apatel@cs.binghamton.edu>
 *
 */

#ifdef MEM_TEST
#include <test.h>
#else
#include <ptlsim.h>
#endif

#include <cpuController.h>
#include <memoryHierarchy.h>

#include <machine.h>

using namespace Memory;

#define DIR_SET 4096
#define DIR_WAY 16
#define DIR_LINE_SIZE 64
#define DIR_ACCESS_DELAY 10
#define REQ_Q_SIZE 128

/**
 * @brief A Directory entry containing information for one line
 */
struct DirectoryEntry {
    bitvec<NUM_SIM_CORES> present;
    bool dirty;
    W64  tag;
    W8   owner;
	bool locked;

    DirectoryEntry() { reset(); }
    void reset();
    void init(W64 tag_);

    ostream& print(ostream &os) const {
        os << "tag:" << (void*)tag << " dirty:" << dirty << " owner:" << owner;
        os << " present:" << present;
        return os;
    }
};

static inline ostream& operator <<(ostream &os, const DirectoryEntry &e)
{
    return e.print(os);
}

/**
 * @brief A Directory containing cacheline informations.
 *
 * This is a singleton class so there is only one Global directory.
 * All directory controllers get access to this directory and should
 * simulate appropriate access delay. This directory is a set-assoc
 * structure. The size of the directory is statically set.
 *
 * TODO:
 *	- Allow configuration options to dynamically set directory size.
 *	- Simulate limited port access
 */
class Directory {
    private:
        Directory();
        static Directory* dir;

        typedef AssociativeArray<W64, DirectoryEntry, DIR_SET,
                DIR_WAY, DIR_LINE_SIZE> base_t;
        typedef FullyAssociativeArray<W64, DirectoryEntry, DIR_WAY,
                NullAssociativeArrayStatisticsCollector<W64,
                DirectoryEntry> > Set;

        base_t* entries;

    public:
        static Directory& get_directory();

        DirectoryEntry *insert(MemoryRequest *req, W64&old_tag);
        DirectoryEntry *probe(MemoryRequest *req);
        int             invalidate(MemoryRequest *req);

        W64 tag_of(W64 addr) { return base_t::tagof(addr); }
};

struct DirContBufferEntry : public FixStateListObject
{
    MemoryRequest  *request;
    DirectoryEntry *entry;
    Controller     *cont;
    Controller     *responder;
    Signal         *wakeup_sig;
    bool            annuled;
    bool            free_on_success;
    bool            shared;
    bool            hasData;
    int             depends;
    int             origin;

    void init() {
        request         = NULL;
        cont            = NULL;
        entry           = NULL;
        annuled         = 0;
        depends         = -1;
        origin          = -1;
        shared          = 0;
        hasData         = 0;
        responder       = NULL;
        wakeup_sig      = NULL;
        free_on_success = 0;
    }

    ostream& print(ostream &os) const {
        if (!request) {
            os << "Free Entry\n";
            return os;
        }

        os << "Request{", *request, "}";
        os << "Cont[", cont->get_name(), "] ";
        if (entry)
            os << "dirEntry[", *entry, "] ";
        else
            os << "dirEntry[None] ";
        os << "depends[", depends, "] ";
        os << "origin[", origin, "] ";
        os << "free_on_success[", free_on_success, "] ";
        os << "annuled[", annuled, "]";
        os << endl;
        return os;
    }
};

static inline ostream& operator << (ostream& os, const
        DirContBufferEntry &entry)
{
    return entry.print(os);
}

/**
 * @brief A Controller interface to access Global Directory
 *
 * All the instances of this controller are connected to each-other
 * for correctness. If the design allows multiple controllers that access
 * the global directory then in case of cache-eviction, the initiating
 * controller can send 'evict' message to all other controllers.
 * In such scenarios, each controller should simulate some delay.
 */
class DirectoryController : public Controller {

    private:
        Directory    &dir_;
        Interconnect *interconn_;

        DirectoryEntry dummy_entries[REQ_Q_SIZE];

        /* Simple function dispatcher to handle memory request */
        typedef bool (DirectoryController::*req_handler)(Message *msg);
        req_handler req_handlers[NUM_MEMORY_OP];

        Signal read_miss;
        Signal write_miss;
        Signal update;
        Signal evict;
        Signal send_update;
        Signal send_evict;
        Signal send_response;
        Signal send_msg;

        static Controller   *controllers[NUM_SIM_CORES];
        static Controller   *lower_cont;

        static DirectoryController *dir_controllers[NUM_SIM_CORES];
    public:
        DirectoryController(W8 idx, const char *name,
                MemoryHierarchy *memoryHierachy);

        static FixStateList<DirContBufferEntry, REQ_Q_SIZE> *pendingRequests_;

        bool handle_interconnect_cb(void *arg);
        void register_interconnect(Interconnect *interconnect,
                int type);
        void print_map(ostream &os);
        void print(ostream &os) const;
        bool is_full(bool flag=false) const;
        void annul_request(MemoryRequest *request);
		void dump_configuration(YAML::Emitter &out) const;

        bool handle_read_miss(Message *message);
        bool handle_write_miss(Message *message);
        bool handle_update(Message *message);
        bool handle_evict(Message *message);

        bool read_miss_cb(void *arg);
        bool write_miss_cb(void *arg);
        bool update_cb(void *arg);
        bool evict_cb(void *arg);
        bool send_update_cb(void *arg);
        bool send_evict_cb(void *arg);
        bool send_response_cb(void *arg);
        bool send_msg_cb(void *arg);

        DirContBufferEntry* add_entry(Message *msg);
        DirContBufferEntry* get_entry(int idx);
        DirContBufferEntry* find_entry(MemoryRequest *req);
        DirContBufferEntry* find_dependent_enry(MemoryRequest *req);
        void wakeup_dependent(DirContBufferEntry *queueEntry);

        DirectoryEntry* get_directory_entry(MemoryRequest *req,
                bool must_present=0);
        DirectoryEntry* get_dummy_entry(DirectoryEntry *entry, W64 old_tag);
};

static inline ostream& operator << (ostream &os, const
        DirectoryController& dir)
{
    dir.print(os);
    return os;
}
