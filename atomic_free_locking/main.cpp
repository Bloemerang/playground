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
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>

#include "PetersonLock.h"
#include "EventBuffer.h"

using std::this_thread::yield;

template <bool fenced>
using LockType = PetersonLock<__typeof__(&yield), fenced>;

using std::mutex;
using unique_lock = std::unique_lock<std::mutex>;
using std::condition_variable;


static void dump_event_buffers(const EventBuffer event_buffer[2], Event::timestamp_t start_time);

/**
 * Await a condition using the specified condition variable and predicate.
 *
 * Supports waiting on a condition which may already be true. Achieved by a combination of
 * waiting and polling.
 *
 * TODO: The code using this function really just wants a semaphore to encapsulate this stuff.
 */
static void await_condition(condition_variable &cond_var, mutex &m, bool &cond)
{
    const auto interval = std::chrono::milliseconds(4);
    auto now = std::chrono::high_resolution_clock::now();
    auto pred = [&cond]() { return cond; };

    unique_lock lock(m);

    while (!(cond || cond_var.wait_until(lock, now + interval, pred))) {
        now = std::chrono::high_resolution_clock::now();
    }
}

/**
 * Pound on the specified lock type for the specified number of iterations.
 */
template <typename Lock>
void exercise_lock(unsigned loop_count)
{
    Lock lock(&yield);
    std::thread thread[2];
    EventBuffer event_buffer[2];

    volatile int shared_value = 0;

    // A mutex to guard the violation handling code.
    mutex require_mutex;

    // A signal for other threads to stop when a thread detects a lock violation.
    bool stop = false;

    // Signaling variables used to detect when other threads have stopped.
    bool done_running[2] = {false,};
    condition_variable done_running_cv;
    mutex done_running_mutex;

    const Event::timestamp_t start_time = mach_absolute_time();

    for (unsigned tid = 0; tid < 2; ++tid) {
        thread[tid] = std::thread([&, tid]()
        {
            EventBuffer &events = event_buffer[tid];

#define REQUIRE(condition) do {                                                                     \
    if (!(condition)) {                                                                             \
        handle_violation("Requirement \"" #condition "\" failed at line %u!\n", __LINE__);          \
        done_running[tid] = true;                                                                   \
        done_running_cv.notify_all();                                                               \
        return;                                                                                     \
    }                                                                                               \
} while (0)

            /**
             * Stop the presses and dump failure info if a lock violation is detected.
             *
             * Releases the lock so other threads can finish up, so the lock had better already be
             * acquired when you call this function.
             */
            auto handle_violation = [&](const char *failure_message, unsigned line)
            {
                if (require_mutex.try_lock()) {
                    // Stop the other threads
                    stop = true;
                    lock.release(tid);
                    await_condition(done_running_cv, done_running_mutex, done_running[!tid]);

                    // Dump failure information
                    printf(failure_message, line);
                    printf("shared_value: %u\n", shared_value);
                    printf("Dumping event buffers:\n");
                    dump_event_buffers(event_buffer, start_time);
                    require_mutex.unlock();
                }
            };

            for (unsigned i = 0; i < loop_count && !stop; ++i) {
                
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
#undef REQUIRE
        });
    }

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
