/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Steven Bloemer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>

#include "PetersonLock.h"
#include "EventBuffer.h"

static void yield()
{
    std::this_thread::yield();
}

static void dump_event_buffers(const EventBuffer event_buffer[2], Event::timestamp_t start_time);

template <bool fenced>
using LockType = PetersonLock<__typeof__(&yield), fenced>;

using std::mutex;
using unique_lock = std::unique_lock<std::mutex>;

template <typename Lock>
void exercise_lock(unsigned loop_count)
{
    Lock lock(&yield);
    mutex require_mutex;
    std::thread thread[2];
    EventBuffer event_buffer[2];
    volatile int shared_value = 0;
    volatile bool start_running = false;
    volatile bool done_running[2] = {false,};

    std::condition_variable start_running_cv,
                             done_running_cv;
    mutex start_running_mutex,
           done_running_mutex;

    const Event::timestamp_t start_time = mach_absolute_time();

#define REQUIRE(condition) do {                                                                     \
    if (!(condition)) {                                                                             \
        lock.release(tid);                                                                          \
        if (require_mutex.try_lock()) {                                                             \
            unique_lock done_running_lock(done_running_mutex);                                      \
            done_running_cv.wait(done_running_lock, [&done_running, tid]() { return done_running[!tid]; }); \
            done_running_lock.unlock();                                                             \
            printf("Requirement \"" #condition "\" failed at line %u!\n", __LINE__);                \
            printf("shared_value: %u\n", shared_value);                                             \
            printf("Dumping event buffers:\n");                                                     \
            dump_event_buffers(event_buffer, start_time);                                           \
            require_mutex.unlock();                                                                 \
        }                                                                                           \
        done_running[tid] = true;                                                                   \
        done_running_cv.notify_all();                                                               \
        return;                                                                                     \
    }                                                                                               \
} while (0)

    for (unsigned tid = 0; tid < 2; ++tid) {
        thread[tid] = std::thread([&, tid]()
        {
            // Wait until the parent thread tells us to start
            {
                unique_lock startup_lock(start_running_mutex);
                start_running_cv.wait(startup_lock, [start_running]() { return start_running; });
            }

            EventBuffer &events = event_buffer[tid];

            for (unsigned i = 0; i < loop_count; ++i) {
                
                LOG(events, "Acquiring lock...");
                lock.acquire(bool(tid));
                LOG(events, "Acquiring lock...done");

                REQUIRE(++shared_value == 1);
                REQUIRE(--shared_value == 0);

                LOG(events, "Releasing lock");
                lock.release(bool(tid));
            }

            done_running[tid] = true;
            done_running_cv.notify_all();
        });
    }

    start_running = true;
    start_running_cv.notify_all();
#undef REQUIRE

    for (unsigned tid = 0; tid < 2; ++tid) {
        thread[tid].join();
    }

    printf("shared_value = %u\n", shared_value);
}

int main(int argc, const char * argv[])
{
    const unsigned loop_count = argc < 2 ? 10'000'000 : atoi(argv[1]);

    printf("Running with %u loops per thread\n", loop_count);

    printf("Exercising Peterson lock with fencing\n");
    exercise_lock<LockType<true>>(loop_count);

    printf("Exercising Peterson lock without fencing\n");
    exercise_lock<LockType<false>>(loop_count);

    return 0;
}

static void dump_event_buffers(const EventBuffer event_buffer[2], Event::timestamp_t start_time)
{
    const unsigned count = 2;

    // Go through the event buffers in parallel, always printing the entry with the latest timestamp
    EventBuffer::ConstReverseIterator itor[count];
    EventBuffer::ConstReverseIterator end[count];

    // Initialize our iterators
    for (unsigned i = 0; i < count; ++i) {
        itor[i] = event_buffer[i].rbegin();
        end[i]  = event_buffer[i].rend();
    }

    unsigned latest_itor = count;

    while (true)
    {
        Event::timestamp_t latest_timestamp = 0;

        // latest_itor remains set to count when all iterators are exhausted
        latest_itor = count;

        // Find the iterator containing the most recent timestamp
        for (unsigned i = 0; i < count; ++i)
        {
            if (itor[i] != end[i] &&
                *itor[i] &&
                itor[i]->timestamp >= latest_timestamp)
            {
                latest_itor = i;
                latest_timestamp = itor[i]->timestamp;
            }
        }

        if (latest_itor == count) {
            // All iterators are exhausted; we're done.
            break;
        }

        (itor[latest_itor]++)->print(latest_itor, start_time);
    }
}
