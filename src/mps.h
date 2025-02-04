/**
@file		mps.h
@version	defined by VERSION_MAJOR and VERSION_MINOR
@author Alexander Kramer
@brief	Messaging/Threading framework

				This framework allows easy, pain-free multi-threading programming with tons of threads without bothering about mutexes, locks etc.

				There are three basic components to understand:
					* pool = On Linux this is basically thread + message queue. This is also known as execution context.
					* message = Data packet. It shouldn't contain any logic. It must be derived from mps::message.
					* worker = Implements processing logic. The method process() must be overwritten and will be called for every message in the pool's queue.
					*               The order of workers in which one message will be processed is not guaranteed. Only that later messages will be processed after earlier ones.
					*               Still current implementation preserves worker order
				Also it provides some other useful classes/methods
				    * ts_queue = thread-safe message queue
					* sleep_ms = this method does sleep of current thread in milliseconds
					* sleep_us = this method does sleep of current thread in microseconds
					* set_this_thread_prio/get_this_thread_prio = sets/gets current thread's locking priority (not scheduling priority)
					* timer = class for easy time-out handling
					* waiter/messagewaiter = class for waiting for specific message in secure way (no dead-lock danger)
					* synchronized = class for thread-safe coping and sharing complex structures
					* distributor = distributes messages across a set of pools
                    * pool_thread = simple execution of a function in a thread
                See file mps_tutorials.cpp for example how to use this framework. You can compile and run it.

*/

#ifndef RB_MPS_H
#define RB_MPS_H

#include "mps_message.h"
#include "mps_synchronized.h"

#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <memory>
#include <queue>
#include <atomic>
#include <set>

#define mps_thread_safe const
#define mps_thread_critical mutable
#define mps_thread_critical_cast const_cast

namespace mps {

    class pool;
    class pool_impl;
    class worker;


// if this is defined, all instances of workers and pools are tracked
#ifdef MPS_TRACK_OBJECTS
    constexpr bool mps_objects_tracking = true;
#else
    constexpr bool mps_objects_tracking = false;
#endif

    constexpr int VERSION_MAJOR = 1;
    constexpr int VERSION_MINOR = 10;

    /// type for mutex lock
    using mps_lock = std::unique_lock <std::mutex>;
    using mps_scope_lock = std::lock_guard <std::mutex>;

    /// sleep for given number of milliseconds
    void sleep_ms(unsigned long ms);

    /// sleep for given number of microseconds
    void sleep_us(unsigned long us);

    /// returns locking priority of current thread
    unsigned int get_this_thread_prio();

    /// sets locking priority of current thread
    void set_this_thread_prio(unsigned int p);

    /**
     * Base mps exception class for all mps exceptions
     */
    class exception : public std::exception {
    protected:
        const char *msg = nullptr;
    public:
        explicit exception(const char *message);

        /// returns exception message
        const char * what() const noexcept override;

        ~exception() override = default;
    };

    /**
     * Exception for wrong locking hierarchy.
     * This exception is thrown if locking hierarchy is violated in one of the wait() methods
     * Waiting pool's priority must always be higher than owning pool's for correct operation
     */
    class locking_exception : public mps::exception {
    public:
        using mps::exception::exception;

        std::string owning_pool; ///< pool which own processing worker
        unsigned int owning_prio = 0; ///< it's priority
        std::string waiting_pool; ///< pool which tried to wait
        unsigned int waiting_prio = 0; ///< it's priority
    };

    /**
     * Exception is throw if thread needs higher priority but user has insufficient rights.
     * Be carefully with high prio threads, as they require sudo.
     */
    class insufficient_privileges : public mps::exception {
    public:
        insufficient_privileges();
    };

    /**
     *  Base class for main mps components:  worker, pool
     *  It can track instantiated objects if mps_objects_tracking is true.
     *  This might be usable to track which pools and workers are not freed correctly.
     */
    class base {
        std::string nodename;
#ifdef MPS_TRACK_OBJECTS
        static std::set<base *> debug_insts;
        static std::mutex debug_mutex;
#endif

    public:

        base();

        base(const base & b) = delete;
        base& operator=(const base & b) = delete;

        virtual ~base();

        /// sets name of the instance
        void node_name(const std::string & str);

        /// returns name of the instance
        std::string node_name() const;

        /// dumps all active instances (if MPS_TRACK_OBJECTS was defined)
        static void dump_all_instances(std::ostream & strstream);

    };


    /// this message will be given to process method if pool->push_back_notification() is called
    class notification_message : public message {
    public:
        notification_message()  = default;
    };


    /**
     * Interface for message processing classes
     */
    class i_messages_acceptor {
    public:

        /**
         * Pushes a message to the objects queue
         * @param m message
         * @return number of messages on the queue
         */
        virtual size_t push_back(std::shared_ptr<const mps::message> m) mps_thread_safe = 0;

        /**
         * Conditional push of message if queue length not exceeds certain limit. After return check if pushed==true.
         * If pushed=true then message was pushed.
         * @param m message
         * @param limit limit of the queue
         * @param pushed it will be filled with true or false
         * @return number of messages on the queue
         */
        virtual size_t push_back_to_limit(std::shared_ptr<const mps::message> m, size_t limit, bool &pushed) mps_thread_safe = 0;

        virtual ~i_messages_acceptor() = default;
    };

    /**
     * Worker implements logic code for handling messages in the process method.
     * You should derive from this class and re-implement own "process" method.
     * You can name your worker with method node_name()
     */
    class worker : public base {
        /// flag to show this worker is already added to a pool
        std::atomic_bool owned;

        /// pointer to owning pool. On add set the flag first, then set this. On remove reset ptr first, then reset the flag
        std::weak_ptr <pool> owner_pool;
    protected:

        /// returns pool where this worker lives
        inline std::weak_ptr <pool> get_owner_pool() const
        {
            return owner_pool;
        }

    public:

        worker() {
            owned.store(false);
            node_name("worker");
        }

        /// abstract method which must be overwritten in subclasses
        virtual void process(std::shared_ptr<const message> m) = 0;

        friend class pool;
        friend class pool_impl;
        friend class distributor_impl;
    };

    // short cut for worker shared ptr
    using worker_sptr = std::shared_ptr<mps::worker>;

    /// interface to add/remove workers
    class i_worker_pool {
    public:

        /**
         * Adds worker to the pool. This will be done by pushing special message
         * @param w worker
         * @return number of messages on the queue
         */
        virtual size_t add_worker(std::shared_ptr<worker> w) mps_thread_safe = 0;

        /**
        * Removes worker from the pool. This will be done as message push
        * @param w worker
        * @return number of messages on the queue if worker belong to the pool
         * Note: if worker doesn't belong to the pool, no error is issued and worker remains potentially active in his owning pool
         * The return value will be 0 in this case.
        */
        virtual size_t remove_worker(std::shared_ptr<worker> w) mps_thread_safe = 0;

        virtual ~i_worker_pool() = default;
    };


    /**
     * Thread safe queue. Template type defines the element type. This queue is used by pool for holding messages
     */
    template<typename T>
    class ts_queue {
    private:
        using queue_type = std::queue <T>;
        mps_thread_critical queue_type queue;
        mps_thread_critical std::mutex mutex;
        mps_thread_critical std::condition_variable cond;

        template<bool checkLimit>
        inline size_t push_internal(const T &item, bool clear, size_t limit, bool &pushed) mps_thread_safe {
            mps_lock mlock(mutex);

            if (clear)
                queue = queue_type();

            size_t r = queue.size();

            if ((!checkLimit) || (r < limit)) {
                queue.push(item);
                if (0 == r)
                    cond.notify_one();
                r++;
                pushed = true;
            } else
                pushed = false;

            mlock.unlock();
            return r;
        }

    public:
        /// first= number of elements in the queue, second=true if element was popped
        using pop_result = std::pair<size_t, bool>;

        /// pops a message with given timeout.

        /// timeout_ms < 0 :  infinite wait
        /// timeout_ms == 0 : no wait
        /// timeout_ms > 0 : wait in ms
        pop_result pop(T &item, int timeout_ms) mps_thread_safe {
            mps_lock mlock(mutex);
            pop_result r(0, false);

            if (timeout_ms != 0 && queue.empty()) {
                    if (timeout_ms > 0)
                        cond.wait_for(mlock, std::chrono::milliseconds(timeout_ms));
                    else
                        cond.wait(mlock); // timeout_ms < 0, ignore sonarLint finding about condition argument
            }

            if (queue.empty()) {
                // return empty result
            } else {
                item = queue.front();
                queue.pop();
                r.first = queue.size();
                r.second = true;
            }

            return r;
        }


        /// pushes item on the queue. Optionally the queue can be cleared before
        size_t push(const T &item, bool clear = false) mps_thread_safe {
            bool pushed;
            return push_internal<false>(item, clear, 0, pushed);
        }

        /// pushes item if size < limit, otherwise not. Check if pushed==true. Then item was pushed.
        size_t push_to_limit(const T &item, size_t limit, bool &pushed) mps_thread_safe {
            return push_internal<true>(item, false, limit, pushed);
        }

    };



    /// pool options used upon pool construction for parametrize behaviour
    class pool_options {

    public:
        static const int INFINITE_WAIT = -1; ///< this is used to indicate infinite wait

        /// Thread scheduling priorities
        typedef enum {
            T_NORMAL = 0, ///< normal thread
            T_LOWER_PRIO, ///< lower prio
            T_HIGHER_PRIO, ///< higher prio, require super user privileges
            T_IDLE ///< Idle scheduling
        } thread_type;

        /// pool will block and wait for messages using this time out. After time out a notification_message is given to process() method
        /// Time is given in milliseconds
        int timeout_wait_for_message = INFINITE_WAIT;

        /// priority is used to define locking hierarchy. Only threads with higher priority are allowed to wait for threads with lower
        /// priority. This prevents circular waits which causes deadlocks. You are encouraged to use mps::waiter for blocking wait operations.
        /// Default value is 100.
        unsigned int priority = 100;

        /// scheduling priority of the thread. (Plattform dependent)
        thread_type type =T_NORMAL;

        /// If true pool will stop automatically if last worker is removed
        bool stop_if_no_workers = false;

        pool_options() = default;
    };

    /// startable interfaces
    class i_startable{
    public:

        /// starts execution of the module, this includes starting threads
        virtual void start() mps_thread_safe = 0;

        /// stops execution of the module, this will signal to threads to stop but will return immediately
        virtual void stop() mps_thread_safe = 0;

        /// blocks until threads are finished and joined
        virtual void join() mps_thread_safe = 0;

        virtual ~i_startable() = default;
    };

    /// shortcut for pool pointer
    using pool_sptr = std::shared_ptr<mps::pool>;

    /**
     * Pool class is used to create instances of a pool (thread+queue)
     * It implements all interfaces to add/remove workers and to push messages to the queue
     */
    class pool: public base, public i_messages_acceptor, public i_worker_pool, public i_startable {
    public:
        /// create instance of mps::pool class. Use this instead of constructor
        static std::shared_ptr<mps::pool> create(const pool_options &options);

        /// create instance of mps::pool class. Use this instead of constructor
        static std::shared_ptr<mps::pool> create();

        /// returns pool options
        virtual const pool_options & get_options() mps_thread_safe  = 0;

        /// returns current pool inside a pool thread execution context
        static pool *current();

        /// pushed notification message into the queue, returns queue length
        virtual size_t push_back_notification() mps_thread_safe = 0 ;

        /// flushes the queue using timeout (same format as for waiter)
        /// returns true if flush was successful
        virtual bool flush(int timeout_ms) mps_thread_safe  = 0;

        /// dump some info about pool and its thread
        virtual void dump(std::ostream & ostr) = 0;

        /// returns native thread id (on linux and windows)
        virtual int64_t native_thread_id() mps_thread_safe = 0;
    };



    /// Timer class. Returns number of milliseconds in elapsed() method
    class timer {

    public:
        using clock_type = std::chrono::system_clock;
        clock_type::time_point start_time; // starting/reset time

        /// timer will be started on creation per default. Provide false if reset is not desired.
        explicit timer(bool reset = true);

        /// resets the timer
        void reset();

        /// elapsed time in milliseconds since reset/creation
        double elapsed() const;
    };

    /// waiter: worker which waits for first message of specific type.
    /// You can derive this class and override the confirm_message method to be more specific which message to take.
    /// The wait method will only return first confirmed message. Following messages will be ignored until call of reset().
    /// After reset again only first confirmed message is returned in wait.
    template<class T_m>
    class waiter : public worker {
        /// protects confirmed_message
        mps_thread_critical std::mutex mutex;

        /// used to block and notify threads
        mps_thread_critical std::condition_variable cond;

        /// check if waiting is allowed: check priorities of pool which owns the worker and calling pool/thread
        void check() {
            auto sowner_pool = this->get_owner_pool().lock();
            int owner_prio = sowner_pool->get_options().priority;
            int caller_prio = mps::get_this_thread_prio();

            if (caller_prio <= owner_prio) {
                // wrong locking prio: throw exception
                locking_exception le("Wrong locking relation");
                le.owning_pool = sowner_pool->node_name();
                le.owning_prio = owner_prio;
                const pool * const p = pool::current();
                if (p != nullptr)
                    le.waiting_pool = p->node_name();
                else
                    le.waiting_pool = "non-pool";
                le.waiting_prio = caller_prio;

                throw le;
            }
        }

        /// confirmed message is stored here from inside the process method under lock
        std::shared_ptr<const T_m> confirmed_message = nullptr;

    protected:

        void process(std::shared_ptr<const mps::message> m) override {
            auto mm = std::dynamic_pointer_cast<const T_m>(m);
            if (mm != nullptr) {
                mps_scope_lock lock(mutex);

                if (confirmed_message == nullptr && confirm_message(mm)) {
                    confirmed_message = mm;
                    cond.notify_all();
                }
            }
        }

        /// @return true if right message is given (confirmed) so
        /// it will be returned in wait method then.
        /// This method is called under lock so be carefull what to do here.
        virtual bool confirm_message(std::shared_ptr<const T_m>) {
            return true;
        }


    public:

        waiter() = default;

        /// wait for message to arrive, if message is already arrived the wait returns immediately.
        /// @return first confirmed message or nullptr if time-out occurred
        std::shared_ptr<const T_m> wait(int timeout_ms = pool_options::INFINITE_WAIT) {
            check();

            mps_lock  lock(mutex);

            if (confirmed_message == nullptr) {
                if (timeout_ms != pool_options::INFINITE_WAIT)
                    cond.wait_for(lock, std::chrono::milliseconds(timeout_ms));
                else
                    cond.wait(lock); // ignore sonarlint hint
            }

            std::shared_ptr<const T_m> res = confirmed_message;

            return res;
        }

        /// resets the waiting state, so waiter can wait for next message
        void reset() {
            mps_scope_lock  lock(mutex);
            confirmed_message.reset();
        }

        ~waiter() override = default;
    };

    /**
     * Waits for message specified in constructor. Helpful to wait until a message which was pushed to pool got processed
     */
    template<class T_m>
    class messagewaiter : public waiter<T_m> {
    private:
        std::shared_ptr<const T_m> mymessage;
    public:
        explicit messagewaiter(std::shared_ptr<const T_m> m) : mymessage(m) {

        }

    protected:
        bool confirm_message(std::shared_ptr<const T_m> m) override {
            // keep for debugging: std::cout << this << " confirm " << m.get() << " == " << mymessage.get() << "\n";
            return mymessage.get() == m.get();
        }
    };


    /**
     * @brief The distributor class distributes message on workers.
     *  Distributor creates n pools and adds workers in round-robin manner to it.
     *  The messages are also pushed back in round-robin manner.
     *  Which worker receives which message should be considered random.
     *  Usually you should add n identical worker instances just after construction.
     *  The behaviour is very similar to regular pool with minor differences:
     *   1. In contrast to the pool the returned value of "remove_worker" can be zero if worker
          does not belong to any of distributor pools
          2. Order of message processing is not guaranteed any more
     */
    class distributor: public base, public i_messages_acceptor, public i_worker_pool, public i_startable
    {
    public :
        /// dump info about pools inside
        /// @param ostr ostream, provide std::cout for printing on default console
        virtual void dump(std::ostream & ostr) = 0;
        /// creates distributor with n pools
        /// each pool will be created with given options and name
        static std::shared_ptr<distributor> create(size_t n, const pool_options & opts, const std::string & pools_name);
    };

    /**
     * Creates a pool which behaves like a std::thread. There will be a worker which will execute the given function.
     * @param thread_func function to execute. If function ends, thread will be terminated.
     * @param name name of the thread
     * @param options pool options
     * @return pool ptr pool which runs the thread. You can wait for termination by calling pool->join() or just throw away the shared ptr
     */
    std::shared_ptr<mps::pool> pool_thread(const std::function<void()> & thread_func, const std::string & name, pool_options options = pool_options());

}
#endif
