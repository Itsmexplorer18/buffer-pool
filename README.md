# Buffer Pool Manager

This project implements a robust and efficient **Buffer Pool Manager** for database systems, comprising three major components:

1. [LRU-K Replacer](#1-lru-k-replacer)
2. [Disk Scheduler](#2-disk-scheduler)
3. [Buffer Pool Manager](#3-buffer-pool-manager)


Each component is designed with concurrency, performance, and correctness in mind. Below is a breakdown of the functionality and design rationale of each module.

---

## 1. LRU-K Replacer

The **LRU-K (Least Recently Used - K)** replacer is an advanced page replacement policy that blends the strengths of both **LRU** and **LFU** while overcoming their individual limitations.

### Why LRU-K Over LRU and LFU?

| Policy | Problem |
|--------|---------|
| **LRU** | Relies too heavily on **temporal locality**. It may evict a page that was recently used multiple times in favor of a page accessed only once but more recently. |
| **LFU** | Focuses only on **frequency**, ignoring **recency**. This can lead to **cache pollution** where old but frequently accessed pages stay, even if no longer relevant. |
| **LRU-K** | Tracks the **K most recent accesses** using a deque per frame, combining **recency and frequency**. It chooses victims based on the **backward K-distance** (the time since the K-th last access), effectively identifying and evicting truly cold pages. |

###  Design Highlights

- **K-history tracking**: Each page keeps a `deque<timestamp_t>` of its last `K` access times.
- **Eviction decision**:
  - If a page has fewer than `K` recorded accesses, it is considered to have **infinite backward distance**.
  - Among all evictable pages, the one with the **largest K-distance** is chosen.
- **Thread-safe**: All operations are guarded by mutexes to ensure correctness under concurrent access.
- **Fine-grained control**: Pages can be marked as **evictable or non-evictable**, and their history is updated independently of eviction eligibility.

---

## 2. Disk Scheduler

The Disk Scheduler is responsible for coordinating asynchronous disk read and write operations. It consists of four main components:

### 2.1 DiskManager
A simple abstraction that simulates disk I/O operations:
- **ReadPage**: Simulates a disk read by introducing a delay and filling the buffer with page data.  
- **WritePage**: Simulates a disk write with a delay.  
In a real database, this would interact with the physical storage device.

### 2.2 DiskRequest
Encapsulates a single disk I/O request:
- **is_write**: Indicates whether the operation is a write (`true`) or a read (`false`).  
- **data**: Pointer to the page buffer to read into or write from.  
- **page_id**: The page identifier.  
- **callback (std::promise<bool>)**: Allows the caller to asynchronously wait for completion and receive success/failure status.  

This design enables non-blocking disk operations where callers can `std::future.get()` later.

### 2.3 Channel<T>
A **thread-safe queue** abstraction for passing disk requests between producer (Buffer Pool Manager) and consumer (worker thread):
- **Put**: Enqueue a request and notify waiting consumers.  
- **Get**: Block until a request is available or the channel is closed, then dequeue it.  
- **Close**: Mark the channel as closed and wake up all waiting threads.  

Internally uses `std::mutex`, `std::condition_variable`, and `std::queue`.

### 2.4 DiskScheduler
Coordinates the background execution of disk requests:
- **Members**:  
  1. `std::unique_ptr<DiskManager> disk_manager` – the disk interface.  
  2. `Channel<std::unique_ptr<DiskRequest>> request_queue` – the queue for pending requests.  
  3. `std::thread worker_thread` – the dedicated thread for processing requests.  

- **Workflow**:
  - The **Schedule** method enqueues a request into the channel.  
  - The worker thread (started in the constructor) continuously dequeues requests and executes them via the `DiskManager`.  
  - On completion, the request's promise is fulfilled to signal the result back to the caller.  
  - Destructor closes the channel and joins the worker thread to ensure clean shutdown.

This design enables **asynchronous, thread-safe, and ordered** disk access without blocking the main execution flow.

---



## 3. Buffer Pool Manager
The **Buffer Pool Manager** acts as an intermediary between memory and disk, managing a pool of frames (in-memory pages) and coordinating:
ongoing

---

