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
#ifndef _event_buffer_h
#define _event_buffer_h

#include <cstdint>
#include <mach/mach_time.h>

/// Log an Event for later examination. See Event::print for how format arguments are passed.
#define LOG(buf, fmt, args...) \
    (buf).push({ "%6llu: [%3u] line %3u: " fmt "\n", mach_absolute_time(), __LINE__, ##args })

////////////////////////////////////////////////////////////////////////////////////////////////////
// Class Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * A simple class that stores information about an event for later printing.
 *
 * Optimized for low overhead logging.
 */
class Event
{
public:
    typedef uint64_t timestamp_t;

    // NOTE: All fields save 'fmt' are left uninitialized for performance. We alway
    // check 'fmt' before accessing other fields, and we always set all fields which
    // a given format string may cause to be read.
    const char  *fmt = nullptr;
    timestamp_t  timestamp;
    unsigned int line;

    int64_t      arg0;
    int64_t      arg1;
    int64_t      arg2;

    explicit operator bool() const { return !!this->fmt; }

    /**
     * Print this Event to stdout, marking it with the specified id number.
     *
     * The start_time parameter is subtracted from this Event's timestamp to provide an easy to
     * read elapsed time.
     *
     * Disallow inlining to facilitate use in debugger
     */
    void print(unsigned id, timestamp_t start_time) const __attribute__((noinline));
};

/******************************************************************************/

/**
 * A simple circular buffer for events in a specified thread.
 *
 * Intended to be used by a single thread for log overhead logging.
 */
class EventBuffer
{
    static constexpr uint32_t BUFFER_SIZE = 256u;
    static constexpr uint32_t BUFFER_SIZE_MASK = BUFFER_SIZE - 1;

    static_assert(BUFFER_SIZE > 0, "BUFFER_SIZE cannot be zero");
    static_assert((BUFFER_SIZE & BUFFER_SIZE_MASK) == 0,
                  "BUFFER_SIZE must be a power of 2 for push() to work");

    uint32_t m_current = BUFFER_SIZE - 1;
    Event m_event[BUFFER_SIZE];

private:
    static uint32_t increment(uint32_t value, int direction)
    {
        // Wrap at the end of the buffer without branching. Guaranteed correct by power-of-two
        // static assertions below.
        return (value + direction) & BUFFER_SIZE_MASK;
    }

public:
    class ConstReverseIterator
    {
    public:
        ConstReverseIterator() = default;
        ConstReverseIterator(const EventBuffer* event_buffer, uint32_t index)
            : m_event_buffer(event_buffer)
            , m_current(index)
        {}

        const Event* operator->() const { return &m_event_buffer->peek(m_current); }
        const Event& operator*()  const { return m_event_buffer->peek(m_current); }

        ConstReverseIterator &operator++();
        ConstReverseIterator  operator++(int);

        /**
         * Compare iterators for equality.
         *
         * Resolves the circular buffer iterator problem by considering two un-incremented
         * iterators to the same index as UNequal. If we didn't do this, the begin() iterator
         * on a full circular buffer would compare equal to the end() iterator. Not the
         * most robust solution, but it handles the common case.
         */
        bool operator==(const ConstReverseIterator&) const;
        bool operator!=(const ConstReverseIterator&) const;
    private:
        const EventBuffer* m_event_buffer = nullptr;
        uint32_t m_current = 0;
        unsigned m_increments = 0;
    };

public:
    /// Append an Event to this buffer, potentially overwriting the oldest event.
    void push(const Event& event)
    {
        m_current = increment(m_current, 1);
        m_event[m_current] = event;
    }

    /// Examine an entry in the buffer. Inlining is disabled to facilitate debugger use.
    const Event& peek(uint32_t index) const __attribute__((noinline)) { return m_event[index]; }
    const Event& peek()               const __attribute__((noinline)) { return peek(m_current); }

    // Iterator starting from the latest event which increments towards older events.
    ConstReverseIterator rbegin() const { return ConstReverseIterator(this, m_current); }

    // Iterator to one past the oldest event.
    ConstReverseIterator rend() const;

    /**
     * Dump the buffer to stdout, marking all entries with the specified id number.
     *
     * The start_time parameter is subtracted from all Events' timestamps, providing easy to
     * read elapsed times.
     *
     * Inlining is disabled to facilitate debugger use.
     */
    void dump(unsigned id,
              Event::timestamp_t start_time = 0,
              uint32_t count = BUFFER_SIZE) const __attribute__((noinline));
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Inline Definitions
////////////////////////////////////////////////////////////////////////////////////////////////////

inline EventBuffer::ConstReverseIterator &
EventBuffer::ConstReverseIterator::operator++()
{
    m_current = EventBuffer::increment(m_current, -1);
    ++m_increments;

    return *this;
}

inline EventBuffer::ConstReverseIterator
EventBuffer::ConstReverseIterator::operator++(int)
{
    ConstReverseIterator original(*this);

    ++(*this);

    return original;
}

inline bool
EventBuffer::ConstReverseIterator::operator==(const ConstReverseIterator &other) const
{
    return m_current == other.m_current && (m_increments > 0 || other.m_increments > 0);
}

inline bool
EventBuffer::ConstReverseIterator::operator!=(const ConstReverseIterator &that) const
{
    return !(*this == that);
}

#endif // _event_buffer_h
