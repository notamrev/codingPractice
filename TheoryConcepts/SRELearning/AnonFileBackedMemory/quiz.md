1. A node is running low on memory. The kernel needs to reclaim pages fast. Rank these in order of "cheapest/fastest to reclaim" to "most expensive/impossible without swap": (a) clean file-backed page, (b) dirty file-backed page, (c) anonymous page, with no swap configured.

cheap to expensive : A -> B -> C with no swap configured


2. You run perf stat on a latency-sensitive service and see a high dTLB-load-misses rate. What's actually happening at the hardware level, and what's one production fix?
Kernel is having high miss rate we can resolve this by implementing HugePages. Large context apps require larger TLB which is not present.


3. Explain what "context switch" does to the TLB, and why frequent context switching on a noisy multi-tenant node can degrade performance beyond just CPU scheduling overhead.
Frequent context switch to the TLB causes the kernel to mark the TLB as old and requires a rewrite for the new process that will implement it. This degrades performance as the CPU has to constantly flush out the page mapping for virtual to physical address mapping then refill that data.


4. A process calls malloc(10GB) on a machine with only 4GB of physical RAM and vm.overcommit_memory=0. Does the call succeed or fail? Explain why, tying it to demand paging.
The call succeeds as the Kernel allows this, the bet is process will only use memory it calls for. This follows demand paging where the TLB is created as the process executes if it requires some data and its not present in RAM then the disk is queried. 


5. What's the difference between a cgroup-level OOM kill (what you triggered Wednesday) and a system-wide OOM kill? Which one can potentially kill a completely unrelated process, and why does that matter on a K8s node?
Cgroup OOMKill invoked by the kernel causes the process which is exceeding its memory.req limit. During execution for a particular pod if the pod takes more memory then that process group is identified and then killed. A system-wide OOM kill however is triggered even if only one pod is resource hungry. The system flags as resources exhausted and scores based on usage. Processes with high usage even if they arent the cause can be the reason for the outage.


6. (Interleaved — Tuesday) A process reads a file for the first time via read(). Is this a minor or major page fault? What if a second process reads the same file right after — minor or major for that second process, and why?
If the data is in RAM and in A page table no fault. If not in ram then not in page table for A hence a major. It is added to both. B when calling the same data faces a minor page fault. This is because the data is in RAM but not in B page table.


7. (Interleaved — K8s internals) You delete a static pod's manifest file from /etc/kubernetes/manifests/ by mistake. What happens to the running pod, and roughly how long before you'd notice via kubectl?
If a static pod manifest is deleted then kubelet will also bring down the pod in the system. This is because the pod does not match the manifest kubelet compares to. We would notice at roughly a 20sec interval or the sync interval run by kubelet. We can check with kubectl.


8. On a K8s node, BestEffort pods get killed first when the node hits memory pressure. Tie this back to what you learned today — what specific kernel-level knob does K8s set to make this happen, and is this decided at the cgroup level or the system level?
Its a system wide OOM Kill with the help of the attribute `oom_score_adj` pods are scored guarenteed burstable and best effort. First pods die at the node level and worst case kubelet is killed too.