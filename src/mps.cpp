/**
 * MPS Framework implementation
 */
#include "mps.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <thread>

#undef KNOWN_PLATFORM

// try to detect platform
#ifndef MPS_THREADS_POSIX
#if defined(_linux) || defined(__unix__)
#define MPS_THREADS_POSIX
#endif
#endif

#ifndef MPS_THREADS_MSVC
#if defined(_MSC_VER) || defined(WIN32) || defined(_WIN32)
#define MPS_THREADS_MSVC
#endif
#endif

#ifndef MPS_THREADS_QT
#ifdef QT
#define MPS_THREADS_QT
#endif
#endif


#ifdef MPS_THREADS_QT

#pragma message("MPS Threads: Qt")

#include <windows.h>

void platform_thread_set_name(const char *)
{

}

void platform_thread_set_schedule_priority(mps::pool_options::thread_type t) {

    auto h = GetCurrentThread();

    switch (t) {
        case mps::pool_options::T_HIGHER_PRIO:
            SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL);
            break;
        case mps::pool_options::T_LOWER_PRIO: {
            SetThreadPriority(h, THREAD_PRIORITY_BELOW_NORMAL);
            break;
        }
        case mps::pool_options::T_IDLE: {
            SetThreadPriority(h, THREAD_PRIORITY_IDLE);
        }
        default:
            return;
    }
}

int64_t platform_get_thread_id()
{
    return GetCurrentThreadId();
}

#define KNOWN_PLATFORM

#endif

#ifdef _MSC_VER
#pragma message("MPS Threads: MSVC")

#include <windows.h>

const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)
void SetThreadName(DWORD dwThreadID, const char* threadName) {
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
    }
#pragma warning(pop)
}

void platform_thread_set_name(const char * n)
{
    wchar_t wtext[20];
    size_t ret;
    mbstowcs_s(&ret, wtext, 20,  n, strlen(n)+1);//Plus null
    LPWSTR ptr = wtext;
    // Starting with Windows 10:
    SetThreadDescription(GetCurrentThread(), ptr);
    SetThreadName(-1, n);
}

void platform_thread_set_schedule_priority(mps::pool_options::thread_type t) {

    auto h = GetCurrentThread();

    switch (t) {
        case mps::pool_options::T_HIGHER_PRIO:
            SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL);
            break;
        case mps::pool_options::T_LOWER_PRIO: {
            SetThreadPriority(h, THREAD_PRIORITY_BELOW_NORMAL);
            break;
        }
        case mps::pool_options::T_IDLE: {
            SetThreadPriority(h, THREAD_PRIORITY_IDLE);
        }
        default:
            return;
    }
}
int64_t platform_get_thread_id()
{
    return GetCurrentThreadId();
}

#define KNOWN_PLATFORM

#endif

#ifdef MPS_THREADS_POSIX

#pragma message("MPS Threads: Posix pthreads")

#include <unistd.h>

void platform_thread_set_schedule_priority(mps::pool_options::thread_type t) {
    struct sched_param param;
    int sched_policy;
    auto native_handle = pthread_self();
    sched_policy = SCHED_RR;

    if (t==mps::pool_options::T_NORMAL)
    {
        return;
    }

    switch (t) {
        case mps::pool_options::T_HIGHER_PRIO:
            sched_policy = SCHED_RR;
            param.sched_priority = 80;
            break;
        case mps::pool_options::T_LOWER_PRIO: {
            sched_policy = SCHED_OTHER;
            auto r = nice(10);
            (void) r;
            param.sched_priority = 0;
            break;
        }
        case mps::pool_options::T_IDLE: {
            param.sched_priority = 0;
            sched_policy = SCHED_IDLE;
            auto r = nice(20);
            (void) r;
            break;
        }
        default:
            return;
    }


    auto ret2 = pthread_setschedparam(native_handle, sched_policy, &param);

    if (ret2 != 0) {
        if (ret2 == EPERM)
            std::cout << "ERROR in mps.cpp: Failed to set thread priority " << (int)t << " because of insufficient permissions" << std::endl;
        else
            std::cout << "ERROR in mps.cpp: Failed to set thread priority " << (int)t << ", Error="   << ret2 << std::endl;
        throw mps::insufficient_privileges();
    }
    else {
        //std::cout << "============== OK Set thread priority " << std::endl;
    }


}

#define platform_thread_set_name(tname) { pthread_setname_np(pthread_self(), tname); }

int64_t platform_get_thread_id()
{
    return (int64_t)(gettid());
}

// Linux implementation ends
#define KNOWN_PLATFORM

#endif

#ifndef KNOWN_PLATFORM
#pragma message("MPS Threads: Unknown")
#warning "MPS was not able to identify platform, some features will be not available"
// fallback for unknown platform

void platform_thread_set_schedule_priority(mps::pool_options::thread_type t) {
    (void)t;
}

void platform_thread_set_name(const char*)
{
}

int64_t platform_get_thread_id()
{
    return 0;
}

#endif


namespace mps {

thread_local unsigned int tls_priority = 0;
thread_local pool_impl * tls_pool = nullptr;

/// sleep for given number of milliseconds
void sleep_ms(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

/// sleep for given number of microseconds
void sleep_us(unsigned long us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}
#ifdef MPS_TRACK_OBJECTS
std::set<base *> base::debug_insts;
std::mutex base::debug_mutex;
#endif

exception::exception(const char *message) : msg(message) {

}

const char *exception::what() const noexcept {
    return  msg;
}

insufficient_privileges::insufficient_privileges(): exception("Insufficient privileges") {

}


/// ---------------------- Base ----------------------
    base::base(){
#ifdef MPS_TRACK_OBJECTS
        if (mps_objects_tracking) {
            mps_scope_lock lock(debug_mutex);
            debug_insts.insert(this);
        }
#endif

    }

    void base::node_name(const std::string & str) {
        nodename = str;
    }

    /// returns name of the instance
    std::string base::node_name() const {
        return nodename;
    }


    void base::dump_all_instances(std::ostream &strstream) {
        #ifdef MPS_TRACK_OBJECTS
        if (mps_objects_tracking) {
            mps_scope_lock lock(debug_mutex);

            if (debug_insts.empty())
                strstream << "no instances left" << std::endl;
            else
                for (auto p: debug_insts) {
                    auto ppool = dynamic_cast<pool*>(p);
                    if (ppool)
                        ppool->dump(strstream);
                    else
                        strstream << p->node_name() << std::endl;
                }
        } else {
            strstream << "instance tracking is disabled. Enable it by defining MPS_TRACK_OBJECTS" << std::endl;
        }
        #else
        strstream << "mps was compiled without defining MPS_TRACK_OBJECTS, no tracking possible" << std::endl;
        #endif
    }


    base::~base() {
#ifdef MPS_TRACK_OBJECTS
        if (mps_objects_tracking) {
            mps_scope_lock lock(debug_mutex);

            if (debug_insts.find(this) == debug_insts.end()) {
                exit(-2);
            }
            debug_insts.erase(this);
        }
#endif
    }

    /// ---------------------- pool_internal_message ----------------------

    /// internal message type used by pool. Do not use this class directly. It's used by framework internally
    class pool_internal_message : public message {
    public:
        enum class message_type {
            m_add_worker, ///< adds worker to worker container
            m_remove_worker, ///< removed worked from worker container
            m_stop, ///< stops the thread (worker argument is ignored)
            m_unused
        };

        message_type type = message_type::m_unused;
        std::shared_ptr<worker> w; ///< used in combination with add/remove type

        pool_internal_message() = default;
    };


    /// ---------------------- pool_impl ----------------------


    /**
     * Pool encapsulates a thread and a message queue.
     * It implements interfaces i_startable, i_worker_pool, i_messages_acceptor
     * You should name your pools with node_name() method for easier debugging
     * Direct construction is prohibited for good reasons. Use create() method instead.
     * @see tutorials for usage details
     */
    class pool_impl : public pool {
    private:
        using worker_container = std::vector <std::shared_ptr<worker> > ;
        using queue_container =  ts_queue<std::shared_ptr<const mps::message> > ;
        using thread_ptr =  std::shared_ptr <std::thread>;
        int64_t native_handle_id = 0;

        queue_container queue; ///< queue with messages
        pool_options options; ///< copy of options provided in constructor
        size_t last_queue_size; ///< copy of last queue size which is returned by queue_size

        /// container with workers
        mps_thread_critical worker_container workers;

        /// thread pointer
        mps_thread_critical thread_ptr thread;

        /// own smart ptr reference keeps pool during thread execution
        mps_thread_critical std::shared_ptr<pool> own_ref;

        /// used to start thread
        mps_thread_critical std::atomic<int> started;

        /// notification message is reused by pools method push_back_notification
        std::shared_ptr<notification_message> nmessage;

        /// while this is true, thread is running. It is set to false to gracefully and correctly terminate a thread
        bool run;

        /// thread proc given to the std::thread
        static void thread_static_proc(pool_impl *p);

        /// used internally if lock is obtained
        void remove_worker_internal(worker_container::value_type w) {
            auto it = std::find(workers.begin(), workers.end(), w);
            if (it != workers.end()) {
                workers.erase(it);
            }
            else
                return; // don't reset workers owner if in wrong pool

            w->owner_pool.reset(); // reset owner pool pointer
            w->owned.exchange(false); // finally reset the flag
            if (options.stop_if_no_workers && workers.empty()) {
                stop();
            }
        }

        /// used internally if lock is obtained
        void add_worker_internal(worker_container::value_type w) {
            workers.push_back(w);
        }

        /// process internal message like add_worker,remove_worker,stop,...
        void proc_internal_msg(const pool_internal_message *mym);

        /// process message by all workers
        void proc_workers(std::shared_ptr<const mps::message> m) mps_thread_safe;

        /// thread procedure of the instance called by static_thread_proc
        void thread_proc();

        /// used internally
        pool_impl *get_this_critical() mps_thread_safe {
            auto *_this = mps_thread_critical_cast<pool_impl *>(this); // ignore sonarlint hint
            return _this;
        }

        /// called during construction
        void init() {
            nmessage = std::make_shared<notification_message>();
            started.store(0);
            node_name("pool");
            last_queue_size = 0;
        }

        /// his is needed to create short name for thread, as pthreds only support up to this length
        char pool_short_name[16];

        /// Use create method for construction
        explicit pool_impl(const pool_options &options) : options(options) {
            init();
        }
    public:

        /// return queue size. This is not synchronized so it should be used only for statistics
        size_t queue_size() const {
            return last_queue_size;
        }

        ~pool_impl() override {
            if (mps_objects_tracking)
                std::cout << "pool " << node_name() << " destroyed" << std::endl;
        }

        /// pushed message into the queue, returns queue length
        size_t push_back(std::shared_ptr<const mps::message> m) mps_thread_safe override {
            return queue.push(m);
        }


        size_t push_back_notification() mps_thread_safe override {
            return queue.push(nmessage);
        }

        /// conditional push of message if queue length not exceeds certain limit. check if pushed==true. Then message was pushed
        size_t push_back_to_limit(std::shared_ptr<const mps::message> m, size_t limit, bool &pushed) mps_thread_safe override {
            return queue.push_to_limit(m, limit, pushed);
        }

        /// adds worker
        size_t add_worker(std::shared_ptr<worker> w) mps_thread_safe override {
            if (w == nullptr)
                throw exception("worker must not be null");

            if (!w->owned.exchange(true)) {
                auto m = std::make_shared<pool_internal_message>();
                m->type = pool_internal_message::message_type::m_add_worker;
                m->w = w;
                w->owner_pool = own_ref;
                return queue.push(m);
            } else
                throw exception("worker already added");
        }

        /// removes worker
        size_t remove_worker(std::shared_ptr<worker> w) mps_thread_safe override {
            if (w == nullptr)
                throw exception("worker must not be null");

            auto wp = w->get_owner_pool().lock();
            if (wp == nullptr)
                throw exception("worker has no owner");

            if (wp.get()  !=  this)
                return 0;

            auto m = std::make_shared<pool_internal_message>();
            m->type = pool_internal_message::message_type::m_remove_worker;
            m->w = w;
            return queue.push(m);
        }

        /// starts the thread
        /// @see i_startable::start
        void start() mps_thread_safe override;

        /// signals thread to stop
        /// @see i_startable::stop
        void stop() mps_thread_safe override;

        /// joins the thread
        /// @see i_startable::join
        void join() mps_thread_safe override;

        bool flush(int timeout_ms) mps_thread_safe override;

        /// dumps debug infos
        void dump_debug_info(std::ostream &ostr) const {
            ostr << "Use count:" << own_ref.use_count() << std::endl;
        }

        /// returns pool options
        const pool_options & get_options() const override {
            return options;
        }

        /// create instance of derived custom class of pool. Use this instead of constructor
        template<class T>
        static std::shared_ptr<T> create_custom(const pool_options &options) {
            std::shared_ptr<T> myPool{new T(options)}; // cannot be changed to make_shared
            myPool->own_ref = myPool;
            return myPool;
        }

        int64_t native_thread_id() mps_thread_safe override
        {
            return native_handle_id;
        }

        void dump(std::ostream & ostr) override
        {
            ostr << "Pool Name:" << node_name() << ",Thread ID:" << native_thread_id() << ", Use count:" << own_ref.use_count() << ", Workers: " << workers.size() << std::endl;
        }
    };



void pool_impl::start() mps_thread_safe {
    if (started.fetch_add(1) == 0) {
        // keep for debugging:  std::cout << "starting pool " << node_name() << std::endl;
        std::shared_ptr<pool> local_ref(own_ref);
        thread = std::make_shared<std::thread>(thread_static_proc, get_this_critical());
        started.fetch_add(2);
    } else {
        started.fetch_sub(1);
        throw exception("repeated start");
    }
}

void pool_impl::stop() mps_thread_safe {
    // keep for debugging: std::cout << "stopping pool " << node_name() << std::endl;
    auto m = std::make_shared<pool_internal_message>();
    m->type = pool_internal_message::message_type::m_stop;
    queue.push(m);
}

void pool_impl::join() mps_thread_safe {
    if (started.load() == 3) {
        // keep for debugging:  std::cout << "joining pool ... " << node_name() << std::endl;
        thread->join();
        // keep for debugging:  std::cout << "joined pool " << node_name() << std::endl;
    }
}

void pool_impl::thread_proc() {

    std::shared_ptr<const mps::message> m;
    queue_container::pop_result qpr;
    run = true;
    native_handle_id = platform_get_thread_id();

    std::string short_name = node_name();
    if (short_name.size() > sizeof(pool_short_name)-1)
        short_name.resize(sizeof(pool_short_name)-1);

    for (size_t i=0;i<short_name.size();i++)
        pool_short_name[i] = short_name[i];
    pool_short_name[short_name.size()] = 0;

    platform_thread_set_name(pool_short_name);
    platform_thread_set_schedule_priority(options.type);

    /// thread loop will go until pool is stopped
    while (run) {
        qpr = queue.pop(m, options.timeout_wait_for_message);
        last_queue_size = qpr.first;
        if (qpr.second) {
            auto mym = std::dynamic_pointer_cast<const pool_internal_message>(m);
            if (mym) {
                proc_internal_msg(mym.get());
            }
            else {
                proc_workers(m);
                m.reset();
            }
        }
        else {
            proc_workers(nmessage);
        }

    }
}

void pool_impl::proc_workers(std::shared_ptr<const mps::message> m) mps_thread_safe {
    for (auto &w: workers) {
        try {
            w->process(m);
        }
        catch (mps::exception &e) {
            std::cerr << "Worker " << w->node_name() << " caused error: " << e.what() << ". Worker will be removed\n";
            remove_worker(w);
        }
        catch (std::exception &e) {
            std::cerr << "Worker " << w->node_name() << " caused error: " << e.what() << ". Worker will be removed\n";
            remove_worker(w);
        }
        catch (...) {
            std::cerr << "Worker " << w->node_name() << " caused unknown error and will be removed\n";
            remove_worker(w);
        }
    }
}

void pool_impl::thread_static_proc(pool_impl *p) {
    tls_pool = p;
    tls_priority = p->options.priority;

    p->thread_proc();
    if (p->own_ref.use_count() == 1) {
        // we should detached
        p->thread->detach();
    }
    p->own_ref.reset();
}


void pool_impl::proc_internal_msg(const pool_internal_message *mym) {
    switch (mym->type) {
        case pool_internal_message::message_type::m_add_worker:
            add_worker_internal(mym->w);
            break;
        case pool_internal_message::message_type::m_stop:
            run = false;
            break;
        case pool_internal_message::message_type::m_remove_worker:
            remove_worker_internal(mym->w);
            break;
        default:
            break;
    }
}

bool pool_impl::flush(int timeout_ms) const {
    auto nm = std::make_shared<mps::notification_message>();
    auto mw = std::make_shared<mps::messagewaiter<mps::notification_message> >(nm);
    add_worker(mw);
    push_back(nm);
    auto nm2 = mw->wait(timeout_ms);
    remove_worker(mw);
    if (nm2.get() == nm.get()) {
        return true;
    } else
        return false;
}

//------------------------------- pool -----------------------------------

/// create instance of mps::pool class. Use this instead of constructor
std::shared_ptr<mps::pool> pool::create(const pool_options &options)
{
    return pool_impl::create_custom<mps::pool_impl>(options);
}

/// create instance of mps::pool class. Use this instead of constructor
std::shared_ptr<mps::pool> pool::create()
{
    return pool_impl::create_custom<mps::pool_impl>(pool_options());
}

pool *pool::current() {
    return tls_pool;
}


//------------------------------- timer -----------------------------------
timer::timer(bool resetIn) {
    if (resetIn)
        reset();
}

double timer::elapsed() const {
    clock_type::time_point stop_time = clock_type::now();
    auto ms = 0.001*((double) (std::chrono::duration_cast<std::chrono::microseconds>(stop_time - start_time).count()));
    return ms;
}

void timer::reset() {
    start_time = clock_type::now();
}





//------------------------------- get / set / locking prio -----------------------------------
    unsigned int get_this_thread_prio() {
        return tls_priority;
    }

    void set_this_thread_prio(unsigned int p) {
        tls_priority = p;
    }

    class distributor_impl : public distributor
    {
        private:
            mps_thread_critical size_t current = 0;
            mps_thread_critical std::mutex mutex;

        protected:
            /// returns index of the next pool
            /// pool index is incremented on every call
            size_t next_pool() mps_thread_safe
            {
                mps::mps_scope_lock lock(mutex);
                auto r = current;
                current = (current+1)%pools.size();
                return r;
            }

            /// internal pools
            std::vector<std::shared_ptr<pool>> pools;

        public:

            distributor_impl(size_t n, const pool_options & opts, const std::string & pools_name)
            {
                node_name("distributor");
                pools.resize(n);
                for (size_t i=0;i<n;i++)
                {
                    auto p = mps::pool::create(opts);
                    p->node_name(pools_name);
                    pools[i] = p;
                }
            }

            size_t push_back(std::shared_ptr<const mps::message> m) mps_thread_safe override
            {
                return pools[next_pool()]->push_back(m);
            }

            size_t push_back_to_limit(std::shared_ptr<const mps::message> m, size_t limit, bool &pushed) mps_thread_safe override
            {
                return pools[next_pool()]->push_back_to_limit(m, limit, pushed);
            }

            /// starts execution of the module, this includes starting threads
            void start() mps_thread_safe override
            {
                    for (auto & p:pools)
                        p->start();
            }

            /// stops execution of the module, this will signal to threads to stop but will return immediately
            void stop() mps_thread_safe override
            {
                for (auto & p:pools)
                    p->stop();
            }

            /// blocks until threads are finished and joined
            void join() mps_thread_safe override
            {
                for (auto & p:pools)
                    p->join();
            }

            void dump(std::ostream & ostr) override
            {
                for (auto & p:pools)
                    p->dump(ostr);
            }

            size_t add_worker(std::shared_ptr<worker> w) mps_thread_safe override
            {
                if (w == nullptr)
                    throw exception("worker must not be null");

                return pools[next_pool()]->add_worker(w);
            }

            // in contrast to the pool the returned value can be zero if worker
            // does not belong to the distributor
            size_t remove_worker(std::shared_ptr<worker> w) mps_thread_safe override
            {
                if (w == nullptr)
                    throw exception("worker must not be null");

                auto wp = w->get_owner_pool().lock();
                if (wp == nullptr)
                        throw exception("worker has no owner");

                for (auto & p:pools) {
                    if (p.get() ==wp.get())
                        return p->remove_worker(w);
                }
                return 0;
            }

            ~distributor_impl() override
            {
                pools.clear();
            }
    };


    std::shared_ptr<distributor> distributor::create(size_t n, const pool_options & opts, const std::string & pools_name)
    {
        return std::make_shared<distributor_impl>(n, opts, pools_name);
    }


    class function_call_worker : public worker {
    public:
        std::function<void()> f;
        void process(std::shared_ptr<const mps::message> m) override {
            (void)m;
            f();
        }
    };

    std::shared_ptr<mps::pool> pool_thread(const std::function<void()> & thread_func, const std::string & name, pool_options options) {
        auto p = mps::pool::create(options);
        p->node_name(name);
        auto w = std::make_shared<function_call_worker>();
        w->f = thread_func;
        p->add_worker(w);
        p->start();
        p->push_back_notification();
        p->stop();
        return p;
    }

}
