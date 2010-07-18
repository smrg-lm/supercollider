//  server scheduler
//  Copyright (C) 2008, 2010 Tim Blechmann
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; see the file COPYING.  If not, write to
//  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//  Boston, MA 02111-1307, USA.

#ifndef SERVER_SCHEDULER_HPP
#define SERVER_SCHEDULER_HPP

#include "dsp_thread_queue/dsp_thread.hpp"
#include "group.hpp"
#include "memory_pool.hpp"
#include "utilities/branch_hints.hpp"
#include "utilities/callback_system.hpp"
#include "utilities/static_pooled_class.hpp"

namespace nova
{

/** audio-thread synchronization callback
 *
 *  callback for non-rt to rt thread synchronization. since it is using
 *  a locked internal memory pool, instances should not be allocated
 *  from the real-time thread
 *
 * */
struct audio_sync_callback:
    public static_pooled_class<audio_sync_callback, 1<<20 /* 1mb pool of realtime memory */,
                               true>
{
    virtual ~audio_sync_callback()
    {}

    virtual void run(void) = 0;
};

struct nop_hook
{
    void operator()(void) {}
};

/** scheduler class of the nova server
 *
 *  - provides a callback system to place callbacks in the scheduler
 *  - manages dsp threads, which themselves manage the dsp queue interpreter
 *
 *  scheduler_hook: functor to be called when after callbacks have been executed
 *                  and before the threads are executed
 *
 *  thread_init_functor: helper thread initialization functor
 *
 * */
template<class scheduler_hook = nop_hook,
         class thread_init_functor = nop_thread_init
        >
class scheduler:
    scheduler_hook
{
    typedef nova::dsp_threads<queue_node, thread_init_functor, rt_pool_allocator<void*> > dsp_threads;
    typedef typename dsp_threads::dsp_thread_queue_ptr dsp_thread_queue_ptr;
    typedef typename dsp_threads::thread_count_t thread_count_t;

    struct reset_queue_cb:
        public audio_sync_callback
    {
    public:
        reset_queue_cb(scheduler * sched,
                       dsp_thread_queue_ptr & qptr):
            sched(sched), qptr(qptr)
        {}

        void run(void)
        {
            sched->reset_queue_sync(qptr);
            /** todo: later free the queue in a helper thread */
        }

    private:
        scheduler * sched;
        dsp_thread_queue_ptr qptr;
    };

protected:
    /* called from the driver callback */
    void reset_queue_sync(dsp_thread_queue_ptr & qptr)
    {
        threads.reset_queue(qptr);
    }

public:
    /* start thread_count - 1 scheduler threads */
    scheduler(thread_count_t thread_count = 1, bool realtime = false):
        threads(thread_count, thread_init_functor(realtime))
    {
        threads.start_threads();
    }

    ~scheduler(void)
    {
        cbs.run_callbacks();
        threads.terminate_threads();
    }

    void add_sync_callback(audio_sync_callback * cb)
    {
        cbs.add_callback(cb);
    }

    /* called from the audio driver */
    void operator()(void)
    {
        cbs.run_callbacks();
        scheduler_hook::operator()();
        threads.run();
    }

    /* schedule to set a new queue */
    void reset_queue(dsp_thread_queue_ptr & qptr)
    {
        add_sync_callback(new reset_queue_cb(this, qptr));
    }

private:
    callback_system<audio_sync_callback> cbs;
    dsp_threads threads;
};

} /* namespace nova */

#endif /* SERVER_SCHEDULER_HPP */

