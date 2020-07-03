# Persistent Multi-Word Compare-and-Swap (PMwCAS) for NVRAM

[![Build Status](https://dev.azure.com/haoxiangpeng/pmwcas/_apis/build/status/sfu-dis.pmwcas?branchName=dev)](https://dev.azure.com/haoxiangpeng/pmwcas/_build/latest?definitionId=4&branchName=dev)

PMwCAS is a library that allows atomically changing multiple 8-byte words on non-volatile memory in a lock-free manner. It allows developers to easily build lock-free data structures for non-volatile memory and requires no custom recovery logic from the application. More details are described in the following [slide deck](http://www.cs.sfu.ca/~tzwang/pmwcas-slides.pdf), [full paper](http://justinlevandoski.org/papers/ICDE18_mwcas.pdf) and [extended abstract](http://www.cs.sfu.ca/~tzwang/pmwcas-nvmw.pdf):

```
Easy Lock-Free Indexing in Non-Volatile Memory.
Tianzheng Wang, Justin Levandoski and Paul Larson.
ICDE 2018.
```
```
Easy Lock-Free Programming in Non-Volatile Memory.
Tianzheng Wang, Justin Levandoski and Paul Larson.
NVMW 2019.
Finalist for Memorable Paper Award.
```

## Build
Suppose we build in a separate directory "build" under the source directory.
```bash
$ mkdir build && cd build
$ cmake -DPMEM_BACKEND=[PMDK/Volatile/Emu] \ # persistent memory backend
        -DDESC_CAP=[4/5/6/...] \ # descriptor capacity
        -DWITH_RTM=[0/1] \ # Use Intel RTX
        ..
$ make -jN
```

#### Descriptor size
By default each descriptor can hold up to four words. This can be adjusted at compile-time by specifying the `DESC_CAP` parameter to CMake, for example the following will allow up to 8 words per descriptor:
```
$ cmake -DDESC_CAP=8 [...]
```

#### Persistent memory backends
The current code supports three variants with different persistence modes:

1. Persistence using Intel [PMDK](https://pmem.io)
2. Persistence by emulation with DRAM
3. Volatile (no persistence support)

A persistence mode must be specified at build time through the `PMEM_BACKEND` option in CMake (default PMDK).


## APIs

The central concept of PMwCAS is desrciptors. The typical steps to change multiple 8-byte words are:
1. Allocate a descriptor;
2. Fill in the descriptor with the expected and new values of each target word;
3. Issue the PMwCAS command to actually conduct the operation.

The target words often are pointers to dynamically allocated memory blocks. PMwCAS allows transparent handling of dynamically allocated memory depending on user-specified policy. For example, one can specify to allocate a memory block and use it as the 'new value' of a target word, and specify that this memory block be deallocated if the PMwCAS failed.

See [APIs.md](./APIs.md) for a list of useful APIs and memory related polices and examples.

## Thread Model

It is important to note that PMwCAS uses C++11 thread_local variables which must be reset if the descriptor pool is destructed and/or a thread is (in rare cases) re-purposed to use a different descriptor pool later. The library provides a `Thread` abstraction that extends `std::thread` for this purpose. The user application is expected to use the `Thread` class whenever `std::thread` is needed. `Thread` has the exactly same APIs as `std::thread` with an overloaded join() interface that clears its own thread-local variables upon exit. The `Thread` class also provides a static `ClearRegistry()` function that allows the user application to clear all thread local variables. All the user application needs is to invoke this function upon changing/destroying a descriptor pool.

For example, the below pattern is often used in our test cases:

```
DescriptorPool pool_1 = new DescriptorPool(...);  // Create a descriptor pool
... use pool_1 ...
delete pool_1;
Thread::ClearRegistry();  // Reset the TLS variables, after this it is safe to use another pool

DescriptorPool *pool_2 = new DescriptorPool(...);
... use pool_2 ...
Thread::ClearRegistry();
```

## Benchmarks

We seperate the benchmark code into a independent [repo](https://github.com/sfu-dis/pmwcas-benchmarks) to automatically and continously track PMwCAS performance.

Check out our [benchmark repo](https://github.com/sfu-dis/pmwcas-benchmarks) for more details.

# Contributing

We welcome all contributions. 

## License

Licensed under the MIT License.
