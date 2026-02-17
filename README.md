# High-Performance Dynamic Memory Allocator (C/ARM64)

A custom, thread-safe 64-bit memory allocator optimized for ARM64 architecture (Apple Silicon), designed for high-throughput systems.

## 🚀 Performance Benchmarks

- **Throughput:** ~6.08M operations per second.
- **Latency:** Optimized for $O(1)$ search time via segregated free lists.
- **Concurrency:** 8-thread parallel stress test stability.
- **Efficiency:** 85%+ memory utilization via immediate coalescing.

## 🛠 Technical Highlights

- **Architecture:** Segregated Free Lists (20 power-of-two buckets).
- **Fragmentation Control:** Boundary Tagging and Constant-time coalescing.
- **Safety:** Thread-safe implementation using POSIX Mutexes.
- **Low-Level Logic:** 16-byte alignment and raw `mmap` heap management.

## 💻 Building and Running

1. Clone the repository.
2. Compile the driver: `make`
3. Run the stress test: `./mdriver`

Developed as a demonstration of pointer fluency and systems-level programming at UW-Madison.
