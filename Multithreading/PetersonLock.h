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
#ifndef _peterson_lock_h
#define _peterson_lock_h

#include <cstdint>
#include <cassert>

/**
 * An atomic-free lock useful for synchronizing two (and only two!) threads on an x86 system.
 *
 * Informed by Bartosz Milewski's article on the subject, found while debugging a very similar
 * problem to which he provided the solution. The article is here:
 * http://bartoszmilewski.com/2008/11/05/who-ordered-memory-fences-on-an-x86/
 *
 * The core of the algorithm is that both threads set their own flag to announce their presence
 * and then check whether the other thread's flag is set. It turns out, though, that x86
 * doesn't guarantee the ordering of the write-and-then-read, which breaks the algorithm.
 *
 * This implementation forces the ordering with a memory fence conditional on a template
 * parameter, enabling test code to exercise the lock both with and without the fence.
 *
 * The function used to delay while spinning on the lock is also abstracted by a template
 * parameter.
 */
template <typename WaitFunction, bool fenced>
class PetersonLock
{
    /// The function used to wait while spinning for the lock.
    WaitFunction m_wait_function;

    /// For both threads, whether the thread is currently acquiring or has acquired the lock.
    bool m_interested[2];

    /**
     * Which thread has priority for the lock. Has the bool type only because its range
     * is restricted to 0-1. This field does not indicate a condition per se.
     */
    bool m_thread_priority;

public:
    PetersonLock(WaitFunction wait_function)
        : m_wait_function(wait_function)
    {
        // Both threads are initially uninterested
        m_interested[0] = m_interested[1] = false;

        // No point in initializing m_thread_priority; no path reads it without first writing it.
    }

    /// Acquire the lock for the specified thread (0 or 1), spinning until it is available.
    void acquire(bool thread)
    {
        assert(!m_interested[thread]);

        const bool other_thread = !thread;

        // Announce our interest, but graciously allow the other thread to go first.
        m_interested[thread] = true;
        m_thread_priority = other_thread;

        if (fenced) {
            asm volatile("mfence" ::: "memory");
        }

        // Now that we've announced our interest, wait until the lock is available to us.
        while (m_interested[other_thread] && m_thread_priority == other_thread) {
            m_wait_function();
        }
    }

    /// Release the already-acquired lock for the specified thread (0 or 1).
    void release(bool thread)
    {
        assert(m_interested[thread]);

        m_interested[thread] = false;
    }
};

#endif // _peterson_lock_h
