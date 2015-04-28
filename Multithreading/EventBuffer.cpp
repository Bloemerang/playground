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
#include <cstdio>
#include "EventBuffer.h"

void
EventBuffer::dump(unsigned id, Event::timestamp_t start_time, uint32_t count) const
{
    ConstReverseIterator end = rend();
    unsigned n = 0;

    for (ConstReverseIterator current = rbegin(); current != end && n < count; ++current, ++n) {
        current->print(id, start_time);
    }
}

EventBuffer::ConstReverseIterator
EventBuffer::rend() const
{
    // Special case for an empty EventBuffer.
    if (!peek())
    {
        // Return an incremented iterator to m_current so that it compares equal to rbegin().
        // Recall that m_current + 1 is one *before* rbegin() in a reverse iteration.
        unsigned initial = m_current + 1;

        if (initial == BUFFER_SIZE) {
            initial = 0;
        }

        ConstReverseIterator itor(this, initial);
        ++itor;

        return itor;
    }

    // Seek the oldest entry. m_current denotes the newest, m_current-1 the second newest, etc. So
    // the first existing entry following m_current is the oldest.
    uint32_t next = m_current;

    do {
        next = increment(next, 1);
    } while (!peek(next));

    // We found the oldest; now go back one to get an iterator to one past the oldest in a
    // reverse iteration.
    return ConstReverseIterator(this, increment(next, -1));
}

void
Event::print(unsigned id, timestamp_t start_time) const
{
    printf(this->fmt,
           this->timestamp - start_time,
           id,
           this->line,
           this->arg0,
           this->arg1,
           this->arg2);
}
