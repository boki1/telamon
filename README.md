## Wait-free simulation of lock-free algorithms

[![Actions Status](https://github.com/boki1/telamon/actions/workflows/ci.yml/badge.svg)](https://github.com/boki1/telamon/actions/workflows/ci.yml)
[![MIT License](https://img.shields.io/apm/l/atomic-design-ui.svg?)](https://github.com/tterb/atomic-design-ui/blob/master/LICENSEs)

_Implementation of the algorithm described in [this](http://www.cs.technion.ac.il/~erez/Papers/wf-simulation-full.pdf) paper._

_Check the project [wiki](https://boki1.github.io/telamon/) for the details._

---------------

**Summary**

The algorithm is a transformation mechanism which is able to execute a given lock-free algorithm as if it was wait-free, based on a few properties of the lock-free algorithm implementation. The most important of them is to split the stages involved in executing a complete operation of the data structure in such a way that the majority of the operation can be parallelized.

**Problems**

Currently the implementation is making the assumption that `uint_least_64` is large enough in order to prevent ABA.
Other than that, it also does not take into account that the memory allocater is not wait-free.
