# Thread_Library

## Executive Summary
Basic thread library that allow within process concurrent programing. The library implements mesa style monitors and uses an interrupt library, libinterrupt.a, that is included in the repository. A disk scheduler is also included.

## Partner and Team Composition:
Implemented the thread library with a fellow Williams student Mai Toms. We collaborated on both designing the layout of the library and writing code. The interrupt library was provided for us by superiors but all other code was fully designed and implemented by us.

## Outline of Code

### thread.c
Project	flow is	in the following way. Libinit must be called first and creates a thread (the library produces an error otherwise). For every thread created, the library creates a handler thread that 
alls the function the user program wanted to run. Implements handoff locks and condition variables. 
Runnable and blocked threads are held in a queue. One type of queue exists for locks and another for
condition variables.

### thread2.c
An older version of thread.c to allow for backtracking.

### disk.c
Disk scheduler that supports subsequently reads from disk. Scheduler picks read request that is closest to the current disk position. Note that the scheduler does not actually perform reads and/or writes.

### interrupt.h
Defines the hardware interupts used by libinterrupt.a which allow for atomic actions.

### libinterrupt.a
Interupt library exectuable.

### test1.c
Test case: tests various uses of yield.

### test5.c
Test case: bad starts and various ways to crash the lock methods.

### errortest.c
Test case: possible misuses of thread library commands. Prints relevant	error messages	