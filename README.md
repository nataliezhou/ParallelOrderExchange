# Exchange matching engine using various concurrency and parallel computing mechanisms, implemented in C++ and Go

A matching engine allows the matching of buy and sell instrument orders inside an exchange, receiving new 'active' orders that can be matched against 'resting' orders in the order book. We utilize concurrency and synchronization mechanisms to significantly boost performance, speed, scalability and resource utilization of all our cores.

## C++ Implementation
Our order book is structured as a map of various instruments to their corresponding list of orders to achieve instrument-level concurrency. We implement thread-safe maps, linked lists, and utilize atomics/mutexes to achieve concurrency safely.
We support phase-level concurrency, enabling orders for different instruments to execute concurrently, akin to the single-lane-bridge classic synchronization problem. 

Our order book is separated into different buckets for each instrument. Orders from different instruments can execute concurrently in separate parts of a TSMap. With
traversal-based operations, all orders can be executed concurrently regardless of type. 

#### Further explanation and architecture design is detailed within the c++ directory, where engine.cpp houses our main engine code.

## Go Implementation
The same concurent order book engine is also implemented using Go concurrency structures, including goroutines, channels, and channel peripherals. 
Our order book is composed of individual instrument/side order books, stored locally in multiple worker goroutines. With the order book separated into goroutines for each instrument/side, orders of different instruments and sides can be processed concurrently.

Without a singleton order book, the fan-in/fan-out pattern is applied by using a multiplexer to multiplex orders from multiple clients onto multiple workers.

#### Further explanation and architecture design is detailed within the Go directory, where the main engine code is housed in engine.go.

