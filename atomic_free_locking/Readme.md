This project showcases an atomic-free lock for a finite number of threads (currently two)
on the x86 architecture. The nifty part is that it exposes one of very few "relaxed" aspects
of the x86 memory model: that loads may be reordered with stores to different memory locations.
This project demonstrates the lock both with and without the needed memory fence, and has
instrumentation to catch lock violations and print a history of what happened.

Credit for the central ideas belongs to http://bartoszmilewski.com/2008/11/05/who-ordered-memory-fences-on-an-x86/,
which I ran across while debugging a similar issue for which the article described the root cause. The
implementation, especially the low overhead event logging, is my own.
