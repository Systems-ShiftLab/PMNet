# PMNet Workloads
Workloads used in PMNet's evaluation



## Build Environment/Dependencies
1. Ubuntu 20.04 Kernel 5.8
2. PMDK 1.6 (source included in the directory)

## Running workloads
1. Build the server; Redis-based workload are compatible with normal Redis release (tested with Redis-5.0.8 and [Pmem-Redis](https://github.com/pmem/pmem-redis))       
1.1 For Redis-based workloads, Redis proxy (adaptor) is needed to support PMNet protocol, this is not necessary for Baseline setup that directly connects to the Redis-server with TCP; Run Server-adaptor.
2. Build the client; The Makefile should work.
3. Run the run script in client folder.