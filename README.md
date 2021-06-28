## Wait-free simulation of lock-free algorithms

[![Build Status](https://travis-ci.com/boki1/telamon.svg?token=ryUbS3gNwVxpTHwa5i5x&branch=master)](https://travis-ci.com/boki1/telamon)
[![MIT License](https://img.shields.io/apm/l/atomic-design-ui.svg?)](https://github.com/tterb/atomic-design-ui/blob/master/LICENSEs)

_Implementation of the algorithm described in [this](http://www.cs.technion.ac.il/~erez/Papers/wf-simulation-full.pdf) paper._

---------------

**Summary**

The algorithm is a transformation mechanism which is able to execute a given lock-free algorithm as if it was wait-free, based on a few properties of the lock-free algorithm implementation. The most important of them is to split the stages involved in executing a complete operation of the data structure in such a way that the majority of the operation can be parallelized.

