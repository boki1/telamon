## Telamon

Telamon is a library written in C++20 which aims to execute lock-free algorithms as wait-free. This is achieved by an algorithm with the purpose of monitoring the execution of the operations performed on the given data structure and securing that **each and every thread** is making progress on their tasks.

Telamon is an imlpementation of the algorithm described in depth in [this](http://www.cs.technion.ac.il/~erez/Papers/wf-simulation-full.pdf) paper.

### Summary

The algorithm is a transformation mechanism which is able to execute a given lock-free algorithm as if it was wait-free, based on a few properties of the lock-free algorithm implementation. The most important one is to break-off the stages involved in executing a complete operation of the data structure in such a way that the majority of the operation can be parallelized. There are 3 steps in every operation: prepare, commit, and cleanup. The first and the last can be parellelized, whilst the _commit_ point is where conention may be encountered and has to be executed carefully.

The gist of the algorithm is that every operation is first executed in the so-called _fast-path_, which essentialy represents a regular lock-free execution. If the execution is not sucessfull, the thread-owner of the operation switches to the _slow-path_. Every thread is bound to do that at some point of the algorithm execution. The slow-path is implemented as a mechanism in which the thread-owner of the operation asks for help the other threads and after that repeats the operation. The key difference between the wait-free and lock-free algorithm is that the failed operation is guaranteed to succeed at some point with the wait-free, but that is not the case when the algorithm is lock-free.

## Code docs

[Here]() can be found the documentation generated for the source code comments.

## References

Great resource for understanding the concepts related to this project were [Jon Gjengset's videos](https://www.youtube.com/watch?v=Bw8-vvtA-E8).
