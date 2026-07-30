## Understanding Memory Handling

### MMU - Memory Management Unit

Every process interacts with only the virtual address space of RAM. CPU and Kernel prevent processes from accessing any physical address since direct interaction there can cause catastrophic change to other programs. 
```
CPU issues virtual address
        │
        ▼
      MMU ──looks up── page table (in RAM, per-process)
        │
        ▼
physical address ──▶ actual RAM cell
```

The MMU is what walks the Page table for every memory call invoked by a process. This is incredibly slow since it happens billions of times per second.
This is where the TLB comes into play.

#### The TLB - caching the page table lookup 
Page table also lives in the RAM so whenever memory is needed you have to access the RAM for the table then for the actual memory you want to access.

CPU wants the memory address -> Locate the Page table -> Read the actual data after the page table access

- Here every single memory access now costs 2 RAM trips instead of one - double your effective memory latency, for every load and store the program ever does.
This affects more than expected since RAM is much slower than the CPU.

The fix: Have a quick access cache, dedicated hardware cache that stores recent virtual -> physical translations, sitting right on the CPU next to the registers
Known as the TLB.
```
CPU needs translation for virtual address X
            │
            ▼
    check TLB first
            │
       ┌────┴────┐
       ▼         ▼
    HIT        MISS
    (fast)   walk page table in RAM (slow),
       │      then CACHE the result in TLB
       ▼         │
   physical addr◀┘
```
CPU wants to read virtual address
1. Check TLB first, right on-chip, nanoseconds.
2. TLB Hit - Use that cache
3. TLB Miss - Walk the page in RAM add to TLB
4. Reads the actual data.

Notes:
- TLB = Hardware cache of recent virtual -> physical translations
- TLB Hit = nanoseconds on chip, TLB Miss = Fall back to page table walk in RAM, then cache the result
- Hugepages exist specifically to reduce TLB misses for large memory processes (fewer bigger pages = fewer entries needed to cover the same memory)
- TLB Thrashing shows up at elevated CPU-per-work (CPI), not directly visible in normal APM - needs perf stat / dTLB-load-misses to actually diagnose.

*Understanding HugePages*
- Standard page size for TLB usually is 4kb, with HugePages this changes upto 512mb to 1gb or 2gb based on the performance of the app and what it requires.
- This decreases the load on the TLB drastically and makes the system handle higher workloads.

When a kernel does context-switch for a process to another process, this causes TLB data to be considered expired. The TLB stores data for some other process. In modern systems this is marked with the PID they are associated to. 
This is the reason why context switching is expensive and sometimes `vmstat` shows TLB Thrashing on high usage nodes.

#### Anonymous vs. File Backed Memory
This is how data for process is loaded when it doesn't exist in memory. This is resolution of Major Page fault.

*File Backed Memory* 
This is when the data is retrieved from a file on disk. Binary code segments, shared libraries and memory-mapped files.

*Anonymous Memory* 
This page data has no backing, it comes from application's runtime

```
                    Page needs to leave RAM (memory pressure)
                              │
              ┌───────────────┴────────────────┐
              ▼                                ▼
       File-backed page                  Anonymous page
              │                                │
   clean? just discard it,              nowhere to "discard to" —
   re-read from disk file               MUST go to swap, or it's lost
   when needed again
              │                                │
   dirty? must be written                 written to swap space
   back to disk first                     (disk again, but a
   (writeback)                            dedicated swap area)
```

File-backed pages are cheap eviction as that data can be written and pulled whenever. Anonymous pages have nowhere to go except the RAM and swap.
Page cache is also used-but-reclaimable memory that kernel utilizes. This shows up free but in a memory sync call it is called out as utilization of the entire available memory which is a cause of OOM.


#### Overcommit
The kernel by default is a liar. By `malloc()` if the kernel is requested for 1gb of RAM. It says here you go have 1gb.
The virtual address space and the page table is created for that process. 
The initial physical ram is touched only via demand paging.
```
Process: malloc(1GB)
        │
        ▼
Kernel: "sure, here's 1GB of virtual address space"
        │
        ▼
  (no physical RAM allocated yet)
        │
        ▼
Process writes to byte 0 of that buffer
        │
        ▼
   PAGE FAULT — kernel now allocates ONE physical page (4KB)
        │
        ▼
   (repeats only for pages actually touched)
```
This comes from the bet that the process will only ever touch less RAM than what it asks for and this is what the kernal assumes.
`vm.overcommit.memory` checks this 
0 = heuristic guess (default)
1 = always allow 
2 = strict accounting, never overcommit

Overcommit becomes a common reason of OOMKill since kernel promises the memory and everyone cashes in their requirement at once which is too much for it to handle resulting in the OOMKill of the pod.

#### OOMKill - System Level
There is a OOM that exists system level and not just the cgroup level.
System wide OOM kill differs from the observed and proven cgroup memory group OOM kill we see.

```
Physical RAM + swap exhausted at the WHOLE MACHINE level
(not just one container's cgroup limit)
        │
        ▼
  kernel's OOM killer must free memory NOW
        │
        ▼
  scores every process (oom_score, oom_score_adj)
        │
        ▼
  picks a victim — usually whoever is using the
  most memory and isn't protected (sshd, init are protected)
        │
        ▼
    SIGKILL sent to that process
```
The cgroup OOM is a scoped, per-container version of this same mechanism, the same kernel subsystem, but boundary-limited to one cgroup's `memory.max` instead of the whole machine's physical RAM.
On a K8s node, if you overcommit node-level resource limits badly enough (sum of limits > node capacity) and workloads actually use what they're allowed, you can hit the machine-wide OOM killer.
Which might kill something completely unrelated to the pod that caused the pressure including, in bad cases, kubelet itself. That's why K8s QoS classes (Guaranteed/Burstable/BestEffort) exist, they set `oom_score_adj` so `BestEffort` pods die first at the node level.
malloc() only reserves virtual address space and creates page table entries — no physical RAM is touched yet. Physical RAM only gets allocated page-by-page, on first write to each page (a page fault, same mechanism as demand paging). So the "success" of malloc is really just "the kernel updated some bookkeeping," not "the kernel found you real memory." The bet gets called only when you actually touch the memory you asked for.