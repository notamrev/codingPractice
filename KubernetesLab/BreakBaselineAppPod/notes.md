# Understanding Kill Signals

When linux/kernel kills a process, dies from a signal we observe the following exit code: 128 + signal number.

```
SIGKILL = signal 9 -> exit code 128 + 9 = 137 <- OOMKill 
SIGTERM = signal 15 -> exit code 128 + 15 = 143 <- Graceful shutdown timeout
SIGSEGV = signal 11 -> exit code 128 + 11 = 139 <- segfault
```

So when `kubelet` reports a SIGKILL you will see a 137 code next to it.
Contrast to this a 255 is a process ended with an error code and nobody has any more info.
Reason: Unknown is usually thrown here.

```
       Normal OOMKill flow                   What actually happened here
┌────────────────────────────────┐    ┌──────────────────────────────────────┐
│ cgroup memory.max breached     │    │ `podman machine stop`                │
│        ↓                       │    │        ↓                             │
│ kernel OOM killer              │    │ entire VM (podman-machine-default)   │
│        ↓                       │    │ gets torn down abruptly              │
│ SIGKILL → nginx process        │    │        ↓                             │
│        ↓                       │    │ containerd inside it never gets to   │
│ exit code 137                  │    │ report a clean signal/reason         │
│        ↓                       │    │        ↓                             │
│ kubelet: reason=OOMKilled      │    │ kubelet: exit 255, reason=Unknown    │
└────────────────────────────────┘    └──────────────────────────────────────┘
```
`> kubectl get pod -o jsonpath='{.status.containerStatuses[0].lastState}'`
- This helps in getting the container status and understanding what broke the pod and what the associated status code was.

## Simulating OOMKill

There are 2 ways we can do this. 

### 1. Method A (active/external pressure): 
exec into the running container and force an allocation with stress-ng (installed via apk) 
- This simulates "app has a memory leak" or "app is doing something legitimately memory-hungry."

In this we ran a `stress-ng` process. This process requested 100Mib of memory. This is clear violation of the .yaml we used to bring up the pod.

Kernel's cgroup watching this noticed and flagged all the processes listed in it `memory.oom.group` as OOM and terminated the entire pod. This was then brought up as a restart. 

### 2. Method B (passive/config-induced):
Lower the limits.memory below nginx's own steady-state footprint 
- This simulates "someone set limits too aggressively in a manifest/Helm chart," which is actually the more common real-world OOMKill cause.

This implements a rolling restart. The condition for the old pod to be terminated depends on the new pods readiness probe.
The new pod should pass the health probe. If it doesn't it goes into a CrashLoopBackOff. 
This readiness probe and health check has to be defined in the manifest. Else the pod will simply just restart.

#### Understanding CGroup Enforcement:
Every container gets its own cgroup. (A memory budget kernel tracks in real time)

```
Process inside container tries to allocate memory
              │
              ▼
kernel checks: would this push memory.current > memory.max (the limit)?
              │
        ┌─────┴─────┐
        │           │
       NO          YES
        │           │
        ▼           ▼
   allocation    kernel's cgroup OOM killer wakes up
   proceeds      (scoped to THIS cgroup only, not the whole node)
                    │
                    ▼
            group kill is performed
                    │
                    ▼
            sends SIGKILL to the victim
            pod/container where the process runs
                    │
                    ▼
            victim exits with code 137 (128+9)
                    │
                    ▼
        containerd/kubelet observe this and report:
        reason: OOMKilled, exitCode: 137
```

Understanding CrashLoopBackOff:
- This isn't a failure state rather a waiting state where it waits until a success state is achieved.

Also note:
- Events vs. Container Status: kubectl describe's Events section only shows actions (Pulled/Created/Started/Killing), all Normal type even when the cause was OOMKilled — unless kubelet is backing off or the node is evicting. The real why only lives in Last State: Reason/Exit Code. If you only scan for Warning events during triage, you'll miss a real OOMKill that recovered after one restart.

- The "one shared kernel" model: containers never have their own kernel — everything down to the nginx pod shares the single Linux kernel of the podman VM, isolated only by namespaces + cgroups. Worth one line since it's the reason cgroup OOM behavior applies uniformly across nested "containers."