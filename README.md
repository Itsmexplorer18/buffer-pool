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
ongoing figuring out the design


## 3. Buffer Pool Manager
The **Buffer Pool Manager** acts as an intermediary between memory and disk, managing a pool of frames (in-memory pages) and coordinating:
ongoing

why we need a bf manager in first place why not a mmap like ina n os what about the os mmap

---

