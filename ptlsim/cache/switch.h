
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

#define SWITCH_DELAY 2

namespace Memory {

namespace SwitchInterconnect {

    struct QueueEntry : public FixStateListObject
    {
        MemoryRequest *request;
        Controller    *source;
        Controller    *dest;
        void          *m_arg;
        bool           annuled;
        bool           in_use;
        bool           has_data;
        bool           shared;

        void init() {
            request  = NULL;
            source   = NULL;
            dest     = NULL;
            m_arg    = NULL;
            annuled  = 0;
            in_use   = 0;
            has_data = 0;
            shared   = 0;
        }

        void setup(const Message &msg) {
            source   = (Controller*)msg.sender;
            dest     = (Controller*)msg.dest;
            request  = msg.request;
            m_arg    = msg.arg;
            has_data = msg.hasData;
            shared   = msg.isShared;
            request->incRefCounter();
        }

        void fill(Message &msg) const {
            msg.origin   = source;
            msg.dest     = dest;
            msg.request  = request;
            msg.arg      = m_arg;
            msg.hasData  = has_data;
            msg.isShared = shared;
        }

        ostream& print(ostream& os) const {
            if (!request) {
                os << "Free entry";
                return os;
            }

            os << "request[", *request, "] ";
            os << "source[", source->get_name(), "] ";
            os << "dest[", dest->get_name(), "] ";
            os << "inuse[", in_use, "] ";
            os << "annuled[", annuled, "]";
            return os;
        }
    };

    static inline ostream& operator <<(ostream& os, const QueueEntry
            &entry) {
        return entry.print(os);
    }

    static inline QueueEntry& operator <<(QueueEntry& entry, const
            Message &msg) {
        entry.setup(msg);
        return entry;
    }

    static inline Message& operator <<(Message& msg, const
            QueueEntry& entry) {
        entry.fill(msg);
        return msg;
    }

    /**
     * @brief Represent connection to each controller
     *
     * It contains an incoming queue and a flag that indicate
     * if this controller can accept a packet or not.
     */
    struct ControllerQueue {
        bool        recv_busy;
        bool        queue_in_use;
        Controller *controller;
        FixStateList<QueueEntry, 16> queue;

        ControllerQueue() {
            recv_busy    = false;
            queue_in_use = false;
            controller   = NULL;
            queue.reset();
        }
    };

    /**
     * @brief Create a Switch Interconnect between controllers
     *
     * It creates NxN switch with fixed per controller queue.
     */
    class Switch : public Interconnect
    {
        private:
            dynarray<ControllerQueue*> controllers;

            Signal send;
            Signal send_complete;

            int latency_;

        public:
            Switch(const char *name, MemoryHierarchy *memoryHierarchy);
            ~Switch();

            bool controller_request_cb(void *arg);
            void register_controller(Controller *controller);
            int  access_fast_path(Controller *controller,
                    MemoryRequest *request);
            void annul_request(MemoryRequest *request);
            int  get_delay() { return latency_; }
			void dump_configuration(YAML::Emitter &out) const;

            ControllerQueue* get_queue(Controller *cont);

            bool send_cb(void *arg);
            bool send_complete_cb(void *arg);

            void print(ostream& os) const {
                os << "--Switch-Interconnect: ", get_name(), endl;
                foreach (i, controllers.count()) {
                    ControllerQueue *cq = controllers[i];
                    os << "Controller ", cq->controller->get_name(), " ";
                    os << "busy: ", cq->recv_busy, " Queue:", endl;
                    os << cq->queue;
                }
                os << "--End-Switch-Interconnect\n";
            }

            void print_map(ostream& os) {
                os << "Switch Interconnect: ", get_name(), endl;
                os << "\tconnected to: ", endl;

                foreach (i, controllers.count()) {
                    os << "\t\tcontroller[", i, "]: ";
                    os << controllers[i]->controller->get_name(), endl;
                }
            }
    };

    static inline ostream& operator <<(ostream& os, const Switch &sw)
    {
        sw.print(os);
        return os;
    }
};

};
