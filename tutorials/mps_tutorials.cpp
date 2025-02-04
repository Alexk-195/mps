/**
 * MPS Framework Tutorials implementation
 */

// build with build.sh script

#include "mps_tutorials.h"
#include "mps.h"

#include <iostream>
#include <sstream>
#include <array>

namespace mps_extra {
    using namespace mps;

    void tutorial::intro() {
        /// Introduction: Lets start with a pool. A pool is a thread with queue:
        auto p = mps::pool::create();
        /// start it:
        p->start();
        /// it runs in background now. Thread is sleeping and waiting for incoming messages...

        /// now let's add messages and workers

        /// Here is our simple message type:
        class SimpleMessage: public mps::message
        {
        public:
            /// let's use this simple greeting .....
            std::string greeting = "Hello world! Mps is working file...";
        };

        /// and here is the worker
        /// we need to overwrite only one method "process" like below
        class SimpleWorker: public mps::worker
        {
            protected:
             void process(std::shared_ptr<const mps::message> m) override
            {
                /// If there different message types arriving the thread, we need to check of a specific type:
                auto sm = std::dynamic_pointer_cast<const SimpleMessage>(m);
                if (sm)
                {
                    std::cout << sm->greeting << "\n";
                }
            }
        };

        /// create our worker using make_shared
        auto w = std::make_shared<SimpleWorker>();

        /// add it to pool:
        p->add_worker(w);

        /// now if we push a message, our worker will receive it and print greeting
        p->push_back(std::make_shared<SimpleMessage>());

        /// we issue a stop event to stop processing messages. The stop will return immediately.
        p->stop();

        /// nothing is processed after this line, but pool might still be processing previous messages
        /// To wait until all messages are processed, we need to join it
        p->join();
        /// Like with a thread, after joining the thread finished.
    }

    /// our message for tutorial. it stores a string and a number
    class MyMessage : public mps::message {
    public:
        std::string str; ///< some string in message
        int int_value; ///< some integer in message

        MyMessage() : str("Hello "), int_value(42) {}

        MyMessage(const std::string & s, int i = 42) : str(s), int_value(i) {}
    };


    /// our worker for tutorial. it knows my_message and prints it's content
    class MyWorker : public mps::worker {
    public:
        int n1 = 0; ///< some counter for messages
        int n2 = 0;  ///< some counter for messages

        std::ostream & ost;

        explicit MyWorker(std::ostream & ostIn) : ost(ostIn) {

        }

        void process(std::shared_ptr<const mps::message> m) override {
            {
                // cast to specific type: my_message, it must be const here because m is const.
                // always cast and test for derived class first. Remember: You can always cast to base class.
                auto mym = std::dynamic_pointer_cast<const MyMessage>(m);
                if (mym) {
                    // first buffer whole string as we are in a muitithreaded environment and messages can be mixed otherwise
                    std::ostringstream  oss;
                    oss << mym->str.c_str() << mym->int_value << std::endl;
                    ost << oss.str();
                    n1++;
                    return;
                }
            }
            {
                // cast to specific type: notification_message, it must be const here because m is const.
                // notification message is send upon push_back_notification() and in cases if no message is in the queue and some processing is required.
                auto mym = std::dynamic_pointer_cast<const mps::notification_message>(m);
                if (mym) {
                    ost << "Notification message received"<< std::endl;
                    n2++;
                    return;
                }
            }

            // unknown message: print according message
            ost << "Unknown message received" << std::endl;
        }
    };



    void tutorial::tutorial_1() {
        start_tutorial(1);

        // pool is actually a thread with a message queue.
        // we create pool with default options using static method create() or create_custom().
        // we cannot create pool with constructor, as construction has special functionality
        auto p = pool::create();

        // we also need a worker which will work inside the pool.
        // we take our worker defined above

        auto w = std::make_shared<MyWorker>(ost);

        // add worker to pool
        p->add_worker(w);

        // start pool.
        p->start();

        // we schedule our freshly created message
        p->push_back(std::make_shared<MyMessage>("Hello World, the answer is "));

        //  stopping thread is required otherwise join will block
        p->stop();

        // no more messages will be processed from here

        // waiting for thread to finish
        p->join();

        end_tutorial();
    }


    void tutorial::tutorial_2() {
        start_tutorial(2);

        // pool is actually a thread with a message queue
        // we create pool with default options
        auto p = pool::create();

        // we also need a worker which will work inside the pool.
        // we take our worker defined above
        auto w = std::make_shared<MyWorker>(ost);

        // add worker to pool
        p->add_worker(w);

        // We should setup all pointers, references, ports etc on workers before pool starts
        // before this it's still safe to access workers data, e.g.
        w->n1 = 100;
        w->n2 = 200;

        // now actually start
        p->start();
        // now thread might be running, don't access worker data directly from now!

        // we schedule our freshly created message
        p->push_back(std::make_shared<MyMessage>("Hello World, the answer is "));

        // we can add workers "on-the-fly" into running pool:
        auto w2 = std::make_shared<MyWorker>(ost);
        p->add_worker(w2);

        // the causality of events is always guaranteed: only messages scheduled after this will be seen by both workers (see output)

        // we can also schedule internal notification message by a simple call
        // this is faster than creating new notification message.
        // We can use this to do some work in a thread without actually requiring specific message
        p->push_back_notification();

        //  stopping thread is required otherwise join will block
        p->stop();

        // no more messages will be processed from here

        // waiting for thread to finish
        p->join();

        // after joining it's safe to access workers data
        ost << "Result is " << w->n1 + w->n2 + w2->n1 + w2->n2 << std::endl;

        end_tutorial();
    }

    void tutorial::tutorial_3() {
        // Default pool behaviour is to block and wait for messages in the queue.
        // Sometimes it's desired to run workers even in cases of empty queue.
        // This is controlled by pool options. They can be given to constructor.
        // Changes to options after creation are not relevant.
        start_tutorial(3);

        pool_options options;

        // We want to wake up every 500 ms.
        options.timeout_wait_for_message = 500;

        // pool is actually a thread with a message queue
        // we create pool with default options
        auto p = pool::create(options);

        // we also need a worker which will work inside the pool.
        // we take our worker defined above
        auto w = std::make_shared<MyWorker>(ost);

        // add worker to pool
        p->add_worker(w);

        // We should setup all pointers, references, ports etc on workers before pool starts
        // before this it's still safe to access workers data, e.g.
        w->n1 = 100;
        w->n2 = 200;

        // it's even possible to push back messages before starting pool, they will be processed upon start.
        // we schedule our freshly created message
        p->push_back(std::make_shared<MyMessage>("Hello World, the answer is "));

        // now actually start
        p->start();
        // now thread might be running, don't access worker data directly from now!

        // we also need some sleep here otherwise our pool will be closed immediately
        sleep_ms(2100);
        // so now our worker should receive 4 notification messages

        //  stopping thread is required otherwise join will block
        p->stop();

        // no more messages will be processed from here

        // waiting for thread to finish
        p->join();

        // after joining it's safe to access workers data
        ost << "Result is " << w->n1 + w->n2 << std::endl;
        end_tutorial();
    }


    void tutorial::tutorial_4() {
        // Topic: Detached thread: don't wait for termination

        // Default pool behaviour is to block and wait for messages in the queue.
        // Sometimes it's desired to run workers even in cases of empty queue.
        // This is controlled by pool options. They can be given to constructor.
        // Changes to options after creation are not relevant.
        start_tutorial(4);
        {
            pool_options options;

            // We want to wake up every 500 ms.
            options.timeout_wait_for_message = 500;

            // pool is actually a thread with a message queue
            // we create pool with default options using factory
            auto p = pool::create(options);

            // we also can give names to pools,workers and messages like this:
            p->node_name("tutorial_4_pool");

            // we also need a worker which will work inside the pool.
            // we take our worker defined above
            auto w = std::make_shared<MyWorker>(ost);

            // add worker to pool
            p->add_worker(w);

            // We should setup all pointers, references, ports etc on workers before pool starts
            // before this it's still safe to access workers data, e.g.
            w->n1 = 100;
            w->n2 = 200;

            // it's even possible to push back messages before starting pool, the will be processed upon start
            // we schedule our freshly created message
            p->push_back(std::make_shared<MyMessage>("1. Magic number is "));
            p->push_back(std::make_shared<MyMessage>("2. Magic number is "));
            p->push_back(std::make_shared<MyMessage>("3. Magic number is "));
            p->push_back(std::make_shared<MyMessage>("4. Magic number is "));

            // now actually start
            p->start();
            // now thread might be running, don't access worker data directly from now!

            //  stopping thread is required otherwise join will block
            p->stop();
        }
        ost << "Tutorial 4 scope ends\n";

        sleep_ms(2000);

        // we cannot access pool and we cannot add worker any more, but thread might be still executed and will cleanly exit if all messages are processed (check output!)

        // no more messages will be processed from here
       end_tutorial();
    }


    void tutorial::tutorial_5() {
        start_tutorial(5);
        // Using timer for handling time outs

        // create timer and start it
        mps::timer timer1;

        // take a rest...
        mps::sleep_ms(765);

        // check the time
        double t = timer1.elapsed();

        // should be around 765 (if not stepped in debugger)
        ost << "Time elapsed in timer 1:" << t << std::endl;


        // create timer but not start it
        mps::timer timer2(false);

        // take a rest...
        mps::sleep_ms(765);

        // actually start timer now
        timer2.reset();

        // take a rest...
        mps::sleep_ms(123);

        // check the time
        double t2 = timer2.elapsed();

        // should be around 123 (if not stepped in debugger)
        ost << "Time elapsed in timer 2:" << t2 << std::endl;
        end_tutorial();
    }

    void tutorial::tutorial_6() {
        // Demonstrate use of stop_if_no_workers
        start_tutorial(6);

        mps::pool_options opts;
        opts.stop_if_no_workers = true;

        // create pool with this options
        auto p = mps::pool::create(opts);
        p->start();

        // create worker
        auto w = std::make_shared<MyWorker>(ost);
        p->add_worker(w);

        // just push one message for fun...
        p->push_back(std::make_shared<MyMessage>());

        // after removing last worker, pool is stopped automatically
        p->remove_worker(w);

        ost << "Pool is stopped automatically so we can call join to wait for it..\n";
        p->join();

        end_tutorial();
    }


    void tutorial::tutorial_7() {
        // topic of this tutorial: set different priorities for pools and current thread (e.g. main thread)
        start_tutorial(7);

        // worker which will check priority
        class TestPrio : public mps::worker {
            std::ostream & ost;
        public:
            explicit TestPrio(std::ostream & ost) : ost(ost) {

            }

            void process(std::shared_ptr<const mps::message> m) override {
                (void)m;
                // get current pool is safe inside process method, otherwise we should check pool pointer for null_ptr
                const auto mypool = pool::current();
                ost << "pool " << mypool->node_name() << " has priority " << mps::get_this_thread_prio() << std::endl;

                // the above method work from everwhere in the code, not only inside worker.
                // Inside worker we can access owning pool also like this:

                auto mypool2 = this->get_owner_pool().lock();
                ost << "pool " << mypool2->node_name() << " has priority " << mypool2->get_options().priority
                    << std::endl;
            }
        };

        // set prio of current thread (supposed to be main thread)
        mps::set_this_thread_prio(50);

        // use this options with different prios
        pool_options opts;

        // create three pool with different priorities and name them accordingly
        opts.priority = 101;
        auto p1 = pool::create(opts);
        p1->node_name("p1");

        opts.priority = 102;
        auto p2 = pool::create(opts);
        p2->node_name("p2");

        opts.priority = 103;
        auto p3 = pool::create(opts);
        p3->node_name("p3");

        // add workers to each pool
        p1->add_worker(std::make_shared<TestPrio>(ost));
        p2->add_worker(std::make_shared<TestPrio>(ost));
        p3->add_worker(std::make_shared<TestPrio>(ost));

        // start, notify, stop and join all pools

        p1->start();
        p2->start();
        p3->start();

        p1->push_back_notification();
        p2->push_back_notification();
        p3->push_back_notification();

        p1->stop();
        p2->stop();
        p3->stop();

        p1->join();
        p2->join();
        p3->join();

        // print current pool pointer and it's prio. If thread is not mps::pool the pool pointer will be nullptr.
        ost << "current pool ptr is " << pool::current() << ", current thread prio:" << mps::get_this_thread_prio()
            << std::endl;

       end_tutorial();
    }

    void tutorial::tutorial_8() {

        // Topic of this tutorial: locking priorities and waiting for messages.
        // To avoid circular waits (=dead locks) each thread has locking priority.
        // Only thread with higher priority is allowed to block and wait for another thread.
        // This implies that main thread also needs priority set in order to wait for threads.

        // Our worker Bob will create a special message of type "my_message" after 2 seconds from
        // being notified

        // If You step with debugger this tutorial You might get different results because of time outs.

        start_tutorial(8);

        class Bob : public mps::worker {
        public:
            // this is destination Bob will send message to.
            std::shared_ptr<mps::pool> destination;

            void process(std::shared_ptr<const mps::message> m) {
                (void)m;
                // take a sleep
                mps::sleep_ms(2000);
                // send new message to destination
                destination->push_back(std::make_shared<MyMessage>());
            }
        };

        // John is just a waiting worker which will wait for "my_message"
        auto john = std::make_shared<mps::waiter<MyMessage>>();

        // use this options with different prios
        pool_options opts;

        // create three pool with different priorities and name them accordingly
        opts.priority = 101;
        auto p1 = pool::create(opts);
        p1->node_name("p1(bob)");

        opts.priority = 102;
        auto p2 = pool::create(opts);
        p2->node_name("p2(john)");

        auto bob = std::make_shared<Bob>();
        bob->destination = p2;

        p1->add_worker(bob);
        p2->add_worker(john);

        p1->start();
        p2->start();

        // timer will measure time elapsed from now
        mps::timer timer;

        // notification will trigger Bob::process and send message after 2 seconds
        p1->push_back_notification();

        std::shared_ptr<const MyMessage> r;

        try {
            // actually this attempt will fail and generate exception of type locking_exception.
            // This is because main thread has either no priority defined yet or it was defined by previous tutorial to be 50.
            // Only threads with higher priority than owning thread (in our case pool p2) are allowed to wait.
            // This prevents circualar waits and dead-locks caused by them.
            r = john->wait(1);
        }
        catch (const mps::locking_exception & le) {
            // we will land here because main thread prio is set to 0 by default so main thread is not allowed to wait for pools
            ost << "Exception     :" << le.what() << std::endl;
            // waiting pool is in our case the main thread which has no pool, the name of such thread will be non-pool
            ost << "  waiting pool: " << le.waiting_pool << std::endl;
            // this is pool owning john: p2(john)
            ost << "  owning  pool: " << le.owning_pool << std::endl;
            // this is 50 if previous tutorial was run before
            ost << "  waiting prio: " << le.waiting_prio << std::endl;
            // this is 102
            ost << "  owning  prio: " << le.owning_prio << std::endl;
        }

        // set prio of current thread (supposed to be main thread) higher than of p2
        mps::set_this_thread_prio(200);

        // this is not enough time for message to arrive (will arrive in 2 seconds)
        // so r will be nullptr. Take this into account if debugging it!
        r = john->wait(500);

        // check time elapsed since timer created (should be around 500ms)
        double t = timer.elapsed();

        ost << "Waited " << t << " ms for message" << std::endl;

        // check if we got something: actually we shouldn't
        if (r)
            ost << "Unexpected: Received response message" << std::endl;
        else
            ost << "Expected: Didn't receive any message" << std::endl;

        // wait again, this time until message arrives
        r = john->wait();

        // now it should be around 2000ms
        t = timer.elapsed();

        ost << "Waited " << t << " ms for message" << std::endl;

        // now we also should have the message
        if (r)
            ost << "Expected: Received response message" << std::endl;
        else
            ost << "Unexpected: Didn't receive any message" << std::endl;

        // wait for another message: we have to reset the waiter first
        // and we reset the timer also
        timer.reset();
        john->reset();
        // schedule another message in 2 seconds from now
        p1->push_back_notification();
        // wait up to 3 seconds but we will receive message after 2 seconds
        john->wait(3000);
        ost << "Waited " << timer.elapsed() << " ms for message" << std::endl;

        if (r)
            ost << "Expected: Received response message" << std::endl;
        else
            ost << "Unexpected: Didn't receive any message" << std::endl;

        // stop and join all pools
        p1->stop();
        p2->stop();

        p1->join();
        p2->join();

        // break potential circular dependencies if using shared_ptr by clearing them. Use this after join!
        bob->destination.reset();

        end_tutorial();
    }

    void tutorial::tutorial_9() {
        start_tutorial(9);

        // Using synchronized data wrapper for arbitrary data types
        // This should always be used if access from different pools/threads to non-atomic data is performed

        // define some type or use existing one:
        using mydata_type = std::pair<uint64_t,uint64_t>;

        // define synchronized wrapper
        mps::synchronized<mydata_type> synced_uint64_pair;

        // from one thread/pool fill a copy:
        mydata_type copy1;
        copy1.first = 8723587;
        copy1.second = 9834698;

        // update synchronized object
        synced_uint64_pair.synced_copy_from(copy1);

        // in some other thread/pool retrieve the data
        mydata_type  copy2;
        synced_uint64_pair.synced_copy_to(&copy2);

        ost << "Pair data: " << copy2.first << ", " << copy2.second << "\n";

        end_tutorial();
    }

    /// some busy worker which needs two seconds to process a single message
    class busy_worker: public mps::worker
    {
    protected:
        void process(std::shared_ptr<const mps::message>  m) override
        {
            (void)m;
            sleep_ms(2000);
        }
    };


    void tutorial::tutorial_10() {
        start_tutorial(10);
        // Using message waiter and flush method
        // The main thread should block until the message got processed by worker

        // We will use default pool options
        pool_options opts;
        // now main thread will have prio 101, which is higher than 100 (default for pools)
        // this is needed as only higher prio threads are allowed to wait for lower prio threads
        mps::set_this_thread_prio(opts.priority+1);

        auto p = pool::create(opts);
        auto w = std::make_shared<busy_worker>();
        auto m = std::make_shared<MyMessage>();
        m->str = "This is the message we a waiting for";
        auto waiter = std::make_shared<messagewaiter<MyMessage>>(m);

        // add busy_worker and waiter so waiter will receive message after busy_worker
        p->add_worker(w);
        p->add_worker(waiter);

        // we start the timer to check how long we will wait. It should be about 2 seconds because of busy_worker
        timer my_timer;

        p->start(); // start
        p->push_back(m); // push the message

        bool flush_ok = p->flush(1500); // as processing will take about 2 seconds the flush should fail with timeout after 1.5 seconds
        // for info: pool::flush() uses mps::messagewaiter internally
        // flush() will return true here only with timeout > 4000 as flush message will also be delayed by 2 seconds,
        // so total flush time will be 2 times 2 seconds

        ost << "Result of flush: " << (flush_ok ? "TRUE" : "FALSE") << " (Expected FALSE)\n";
        if (flush_ok) {
            ost << "We waited for " << my_timer.elapsed() << " milliseconds (Expected about 2000)\n";
            throw std::runtime_error("Flush should return false");
        }

        // we put a limit for 1 seconds as we have already waiter for 1.5 seconds above, but the limit will be not reached
        auto confirmed_msg = waiter->wait(1000);

        ost << "We waited for " << my_timer.elapsed() << " milliseconds (Expected about 2000)\n";

        if (confirmed_msg)
            ost << "Confirmed message text: " << confirmed_msg->str << "\n";
        else
            ost << "UNEXPECTED: No confirmed message\n";

        p->stop();
        p->join();

        end_tutorial();
    }

    void tutorial::tutorial_11()
    {
         /// Setting different thread scheduling priorities
        start_tutorial(11);

        ost << "If you see error on setting priority below, you might need to run this demo as superuser" << std::endl;
        ost << "This only happens for setting higher scheduling priority" << std::endl;

        mps::pool_options opts;
        opts.timeout_wait_for_message = 505;
        opts.type = mps::pool_options::T_IDLE;

        /// Create 4 pools with different scheduling priority
        /// There should be no notification messages, as we
        /// will push_back messages there faster than timeout. But if main thread gets stuck for few milliseconds,
        /// we will see notification messages.

        /// IDLE pool
        auto pool1 = mps::pool::create(opts);
        pool1->node_name("Idle pool");
        pool1->start();
        pool1->add_worker(std::make_shared<MyWorker>(ost));

        /// NORMAL pool
        opts.type = mps::pool_options::T_NORMAL;
        auto pool2 = mps::pool::create(opts);
        pool2->node_name("Normal pool");
        pool2->start();
        pool2->add_worker(std::make_shared<MyWorker>(ost));

        /// HIGHER prio
        std::shared_ptr<mps::pool> pool3;
        opts.type = mps::pool_options::T_HIGHER_PRIO;
        pool3 = mps::pool::create(opts);
        pool3->node_name("High prio pool");
        pool3->start();
        pool3->add_worker(std::make_shared<MyWorker>(ost));

        /// LOWER prio
        opts.type = mps::pool_options::T_LOWER_PRIO;
        auto pool4 = mps::pool::create(opts);
        pool4->node_name("Low prio pool");
        pool4->start();
        pool4->add_worker(std::make_shared<MyWorker>(ost));

        mps::timer timer;
        do {

            auto msg1 = std::make_shared<MyMessage>();
            msg1->str = "Hello from main, in pool ";
            msg1->int_value = 1;
            pool1->push_back(msg1);

            auto msg2 = std::make_shared<MyMessage>();
            msg2->str = "Hello from main, in pool ";
            msg2->int_value = 2;
            pool2->push_back(msg2);

            auto msg3 = std::make_shared<MyMessage>();
            msg3->str = "Hello from main, in pool ";
            msg3->int_value = 3;
            pool3->push_back(msg3);

            auto msg4 = std::make_shared<MyMessage>();
            msg4->str = "Hello from main, in pool ";
            msg4->int_value = 4;
            pool4->push_back(msg4);

            mps::sleep_ms(500);
        }while(timer.elapsed()<3000);

        pool1->stop();
        pool2->stop();
        pool3->stop();
        pool4->stop();

        // join all pools
        pool1->join();
        pool2->join();
        pool3->join();
        pool4->join();

        end_tutorial();
    }

    void tutorial::tutorial_12()
    {
        start_tutorial(12);
        ost << "Dumping pool infos, using predefined shared pointer declarations" << std::endl;

        mps::pool_options opts;
        opts.timeout_wait_for_message = 100;
        opts.type = opts.T_IDLE;
        // you can use this type mps::pool_sptr as short cut for std::shared_ptr<mps::pool>
        mps::pool_sptr p = pool::create(opts);

        // the same for message ptr
        mps::message_sptr  m = std::make_shared<MyMessage>();

        p->node_name("my pool");

        // same for worker
        mps::worker_sptr  w = std::make_shared<MyWorker>(ost);

        // Negative Exampl:  removing worker from wrong pool will throw exception
        try {
            p->remove_worker(w);
        }
        catch(const mps::exception & e)
        {
            ost << "Exception:" << e.what() << "\n";
        }

        // now really add worker
        p->add_worker(w);

        p->start();

        ost << "Worker response to message:";

        // the function push_back_to_limit will push a message but only if there are less than
        // provided limit.  The variable "pushed" will be true if message was actually added
        bool pushed;
        p->push_back_to_limit(m, 10, pushed);

        // as message queue was empty, pushed must be true
        if (!pushed)
            throw std::runtime_error("Unexpected");

        // give the thread time to start, this is needed to get valid id
        mps::sleep_ms(100);

        // dump pool infos to cout
        p->dump(std::cout);

        // now remove the worker, next dump will show 0 workers
        p->remove_worker(w);

        // give the pool time to remove worker
        mps::sleep_ms(100);

        // dump again, now there should be no workers
        p->dump(std::cout);

        // accessing native thread id. This will be visible on linux in "top"
        // and on windows in sysinternals "process explorer"
        std::cout << "Native thread id: " << p->native_thread_id() << "\n";

        p->stop();
        p->join();
        end_tutorial();
    }


    void tutorial::tutorial_13()
    {
        start_tutorial(13);
        ost << "Using distributor." << std::endl;

        // pool options will be used by all pools inside distributor
        mps::pool_options opts;
        opts.timeout_wait_for_message = 100;
        opts.type = opts.T_IDLE;

        constexpr size_t N = 5;
        // create distributor with 5 pools
        auto p = distributor::create(N, opts,"DistPool");

        std::array<std::shared_ptr<MyWorker>,N> workers;

        std::array<std::string,N> worker_names = {"Worker_0", "Worker_1","Worker_2","Worker_3","Worker_4"};

        for (size_t i=0;i<N;i++) {
           workers[i] = std::make_shared<MyWorker>(ost);
           workers[i]->node_name(worker_names[i]);
           // add workers to the distributor the same ways as for pool
           // they will be added to different pools in round-robin manner.
           p->add_worker(workers[i]);
        }

        p->start();

        // now let us process 2*N messages by distributor,
        // the order how they are processed and printed is more or less random
        for(size_t i=0;i<2*N;i++)
        {
            p->push_back(std::make_shared<MyMessage>("Received Distributor Message: ",(int)i));
        }

        p->stop();
        p->join();

        // now it's safe to access counters of workers:
        // each worker should have counter =2
        ost << "Message counter of each worker (should be 2):\n";

        for (auto & w: workers) {
            ost << "Message counter of " << w->node_name() << ":" << w->n1 << "\n";
        }

        end_tutorial();
    }

    void tutorial::tutorial_14()
    {
        start_tutorial(14);
        ost << "Using pool_thread" << std::endl;
        pool_options opts;
        auto p = mps::pool_thread([this]() {
            auto p = mps::pool::current();
            ost << "I'm in pool " << p->node_name() << std::endl;
        } , "PoolThread", opts);
        p->join();

        // just executing a function without waiting. Thread will remain valid until termination of function or of the whole application
        mps::pool_thread([this]() {
            for (uint32_t i=0;i<10;i++) {
                ost << "Printing numbers in background: " << i << "\n";
                sleep_ms(100);
            }
        },"Numbers");

        // as we don't have the pool to join, we just wait to give the thread the chance to terminate
        sleep_ms(1500);

        end_tutorial();
    }

    void tutorial::walkthrough() {
        intro();
        tutorial_1();
        tutorial_2();
        tutorial_3();
        tutorial_4();
        tutorial_5();
        tutorial_6();
        tutorial_7();
        tutorial_8();
        tutorial_9();
        tutorial_10();
        tutorial_11();
        tutorial_12();
        tutorial_13();
        tutorial_14();
        ost << "Tutorials done" << std::endl;
    }

    void tutorial::start_tutorial(uint32_t nr)
    {
        ost << "================= TUTORIAL " << nr << " ===================\n";
    }
    void tutorial::end_tutorial()
    {
        ost << "===================  END OF TUTORIAL ===================\n";
    }
}


int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    mps_extra::tutorial tut(std::cout);
    tut.walkthrough();
    std::cout << "Remaining instances (should be none): \n";
    mps::base::dump_all_instances(std::cout);
}
