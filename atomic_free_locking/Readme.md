### Overview

This project showcases an atomic-free lock for a finite number of threads (currently two)
on the x86 architecture. The nifty part is that it exposes one of very few "relaxed" aspects
of the x86 memory model: that loads may be reordered with stores to different memory locations.
This project demonstrates the lock both with and without the needed memory fence, and has
instrumentation to catch lock violations and print a history of what happened.

Credit for the central ideas belongs to http://bartoszmilewski.com/2008/11/05/who-ordered-memory-fences-on-an-x86/,
which I ran across while debugging a similar issue for which the article described the root cause. The
implementation, especially the low overhead event logging, is my own.

### Sample Output

<pre>
$ atomic_free_locking 10000000
Running with 10000000 loops per thread
Exercising Peterson lock with fencing
shared_value = 0
Exercising Peterson lock without fencing
Requirement "++shared_value == 1" failed at line 136!
shared_value: 3
Dumping event buffers:
142624210: [  1] line 134: Acquiring lock...done
142597451: [  1] line 132: Acquiring lock...
142597412: [  1] line 139: Releasing lock
142597365: [  1] line 134: Acquiring lock...done
142597365: [  0] line 134: Acquiring lock...done
142597333: [  0] line 132: Acquiring lock...
142597313: [  1] line 132: Acquiring lock...
142597293: [  0] line 139: Releasing lock
142597272: [  1] line 139: Releasing lock
<b>142597253: [  0] line 134: Acquiring lock...done</b>
142597241: [  1] line 134: Acquiring lock...done
142597215: [  1] line 132: Acquiring lock...
142597192: [  1] line 139: Releasing lock
142597192: [  0] line 132: Acquiring lock...
142597169: [  1] line 134: Acquiring lock...done
142597143: [  1] line 132: Acquiring lock...
</pre>
