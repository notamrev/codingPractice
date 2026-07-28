### Virtual Memory

#### Why does virtual memory exist?

If processes wrote directly to physical RAM addresses we would get 2 disasters:
Without virtual memory: 
- Process A write to physical address 0x1000 
Process B also thinks it owns 0x1000 => they fight each other
- If A wants 100mb contigious block but RAM is fragmented into 100 scattered 1mb holes -> Allocation fails. Even though technically 100mb is free

Virtual memory fixes this:
1. Every process gets its own virtual address space - a private contigious-looking range of address (0 to some huge number) that it only believes it owns. The kernel and hardware translate the virtual address to some physical address which the process doesn't see.

#### Pages, frames and the page table

Virtual memory doesn't translate one byte at a time - that is really slow.

Memory is chopped down into fine chunks

- A chunk in the process's virtual space = Page
- The equivalent chunk in physical RAM = a frame
- The mapping "this virtual page lives at the physical frame" is stored per-process in a data structure called the page table

Suppose when a process reads 0x2004 the CPU then:
1. Figure out which page that address falls in (page number + offset within the page)
2. Look up that page number in the page table -> get the physical frame number 
3. Combine frame number + offset -> actual physical address
4. Read the physical address

Step 2 happens for every memory call that without some cache would be really slow. This is where the TLB (Translation Lookaside Buffer) is used.

Key Terminology:
1. Virtual Memory = transalation layer giving each process a privatre contigious-looking address space
2. Page = Fixed size chunk of virtual address space; 
   Frame = Fixed size chunk of physical address space
3. Page table = Per process mapping of page -> frame
4. Fixed page size lets the CPU split a virtual address into `page number | offset` and use the page number as a direct array index - no search needed
5. Translation direction: Virtual address (from process) -> physical address (via page table) - not the reverse 


#### The TLB - caching the page table lookup 
Page table also lives in the RAM so whenever memory is needed you have to access the RAM for the table then for the actual memory you want to access.

CPU wants the memory address -> Locate the Page table -> Read the actual data after the page table access

- Here every single memory access now costs 2 RAM trips instead of one - double your effective memory latency, for every load and store the program ever does.
This affects more than expected since RAM is much slower than the CPU.

The fix: Have a quick access cache, dedicated hardware cache that stores recent virtual -> physical translations, sitting right on the CPU next to the registers
Known as the TLB.

CPU wants to read virtual address
1. Check TLB first, right on-chip, nanoseconds.
2. TLB Hit - Use that cache
3. TLB Miss - Walk the page in RAM add to TLB
4. Readh the actual data.

Notes:
- TLB = Hardware cache of recent virtual -> physical translations
- TLB Hit = nanoseconds on chip, TLB Miss = Fall back to page table walk in RAM, then cache the result
- Hugepages exist specifically to reduce TLB misses for large memory processes (fewer bigger pages = fewer entries needed to cover the same memory)
- TLB Thrashing shows up at elevated CPU-per-work (CPI), not directly visible in normal APM - needs perf stat / dTLB-load-misses to actually diagnose.

#### Demand Paging 
- Don't load a page in the RAM until a process truly needs it.
So how it works is the initial page is loaded for the process to run. It then requests data and the CPU tries to locate it. It runs into a PAGE FAULT (page miss) CPU notices hey this isn't allocated a page then adds the required page. The instruction that failed and faulted is restarted and captures the new page.
This is not an error this is a safe mechanism to find the missing page and add it i.e demand paging.

Notes: The CPU identifies the fault i.e missing page and the kernel is assigned to locate and load it in the RAM.

#### Page fault
Divided based on where the Kernel has to look for the data
The question asked is "is the data already somewhere in the RAM, just not mapped to the page table?"
1. Minor Page fault: Data is already in the RAM. Not in the page table. The kernel adds it to the page table. No disk I/O. Cheap, fast happens often.
2. Major Page fault: Data not in RAM. Kernel has to locate on the disk. Load to RAM then add to page table. Much slower. Process is BLOCKED until resolved.

Example of minor fault: `exec /bin/bash`. Process A caused the kernel to map and load this. Process B also requests the same thing. Kernel has to only assign to Process B's page table.

On production the signals we look for - `vmstat1` : columns `si/so` (swap in/swap out) tell you about swap-related major faults.
- `pidstat -r 1` - Tells about per-process (`minflt/s`) and (`majflt/s`) fault rates.
- Sustained high `majflt/s` on a process, or non-zero `si/so` in `vmstat`, is your signal that a box is disk-thrashing on memory - usually means it's under real memory pressure, not jsut normal demand paging.


#### Page Cache 
When a program needs to access data it is loaded into the RAM from the disk. Since the page is recently used the Kernel assumes this can be cached since memory is free. It caches then serves to Process A.
When Process B also requests the same data it is looked up in the cache then served to Process B. 

#### Dirty Pages
When a process changes a page in RAM which does not match the page on disk, the Kernel marks it with a flag. Any write operation triggers this. Resulting in a need to write back to the disk. This is not done every time a page is marked DIRTY. This is batched. If the computer loses power then yes data is lost. Avoided by `fsync()` force-sync that will write dirty page to the disk when called. 
IF dirty pages pile up then the kernel blocks processes until this backlog is cleared.