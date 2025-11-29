# Multi-Threaded Process Simulator with Message Passing

This project implements a multi-threaded process simulator designed to model concurrent execution, CPU scheduling, blocking operations, and synchronous message passing across multiple nodes (threads). The simulator uses a combination of priority queues, barriers, and message endpoints to manage processes, providing a test-driven framework for operating systems concepts.

## Description

The simulator supports:

- **Concurrent multi-node execution:** Each node (thread) executes a set of processes independently but synchronizes at barriers.
- **CPU scheduling:** Supports shortest-job-first (SJF) or priority-based scheduling for CPU-bound tasks (`DOOP`) and time-based blocking (`BLOCK`).
- **Synchronous message passing:** Processes can send and receive messages (`SEND` and `RECV`) in a deadlock-safe manner using endpoints and per-node queues.
- **Process lifecycle management:** Tracks process states (`NEW`, `READY`, `RUNNING`, `BLOCKED`, `FINISHED`) and maintains execution statistics.
- **Barriers:** Two-phased barriers ensure proper synchronization across threads and prevent re-entry issues.
- **Statistics and reporting:** Collects detailed metrics on execution, blocking, waiting, sends, and receives.

The simulation reads a program description from standard input and outputs a summary of process execution, including timing and message counts.

## Features

### CPU Scheduling and Execution

- Implements a CPU quantum per node.
- Handles `DOOP`, `BLOCK`, `SEND`, `RECV`, `LOOP`, and `HALT` primitives.
- Preemptive scheduling based on process priority or remaining duration for SJF.

### Message Passing

- Deadlock-free synchronous `SEND`/`RECV`.
- Tracks blocked processes and releases them when the matching partner is ready.
- Per-node priority queues to handle message completions in ascending PID order.

### Barrier Synchronization

- Two-phase barrier with mutex and condition variables.
- Ensures that threads cannot re-enter a barrier before all threads have exited the previous phase.
- Supports dynamic adjustment of participating threads.

### Process Management

- Uses priority queues for ready and blocked processes.
- Tracks detailed statistics: runtime, block time, wait time, sends, receives.
- Supports multiple nodes, each executing processes independently.

## Technologies and Libraries

- C
- POSIX Threads (`pthread`)
- Standard libraries (`stdio.h`, `stdlib.h`, `string.h`, `assert.h`)
- Custom priority queue and barrier implementations

## How to Run

1. Compile the project:

```bash
make
```
2. Run the simulator with a program description as input:
```bash
./simulator < program_input.txt
```
3. Output will show process execution logs and a summary of statistics.

## Author
Arash Tashakori


[Website and Contact information](https://arashtash.github.io/)


## License
MIT License

Copyright (c) 2025 Arash Tashakori

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.