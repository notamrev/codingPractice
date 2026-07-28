1. (Conceptual) When you ran kubectl apply -f baseline-nginx.yaml, walk through what happens between your kubectl command and the container actually running on the
node. Name the components involved and their order.
A) When you run the command. The command is sent to the kube-apiserver who verifies who you are and what you require. Then the kube-controllermanager picks up the
task of identifying how many pods are present/replicas required. It notices 0 then controller-manager creates a pod via the api-server but does not assign a node
for it. Kube-scheduler notices this orphan pod and assigns it to a node. Once a node is assigned kubelet then asks containerD to pull the image and spin up the 
container for this image. Post this the kindnet does the ip routing and assigns the pod its own IP. After this kube-apiserver calls CoreDNS and routes baseline-nginx
to this node.

**CORRECTION — Q1 (score 3.5/5):**
Kubernetes components don't "call" each other directly (exceptions: kubectl→apiserver, and kubelet→containerd via CRI). Everything else is a **watch/reconcile loop** — components watch etcd through the apiserver and react to changes. CoreDNS is NOT called by kube-apiserver; CoreDNS watches Service/Endpoints objects itself and reacts.

Corrected flow:
```
kubectl apply ──▶ kube-apiserver (authn/authz, writes Deployment to etcd)
                       │
                       ▼ (watching Deployments)
       Deployment/ReplicaSet controller ── sees 0/1 replicas, creates Pod object (unscheduled)
                       │
                       ▼ (watching for pods with no node)
                 kube-scheduler ── picks a node, writes binding
                       │
                       ▼ (watching for pods bound to its own node name)
                    kubelet ── calls CNI (kindnet) for pod IP + sandbox
                       │        calls containerd (via CRI) to pull image + start container
                       ▼
                container running, kubelet reports status back to apiserver
                       │
       Endpoints controller (watching Service selector + pod readiness)
                       │ writes pod IP into the Service's Endpoints object
                       ▼
     CoreDNS (watching Services) ── serves DNS name → ClusterIP
     kube-proxy (watching Endpoints) ── programs iptables/ipvs DNAT rules
```
Fix the mental model: "X calls Y" → "X watches Y's state and reacts to it," everywhere except kubelet↔containerd.

2. (Conceptual) What's the difference between a static pod and a normal pod (like CoreDNS or kube-proxy)? Specifically — who creates it, and where does its
definition live?
A) A static pod is not brought up by the api-server this, is brought up from the disk with a fixed manifest. This is brought up by kubelet and the data lives
on the disk.

**Q2 — 5/5, correct.** No correction needed. This was yesterday's weak spot and it's now solid.

3. (Diagnostic) You SSH into the Kind control-plane node and delete the kube-apiserver's pod manifest file from disk. What happens to the running kube-apiserver
pod? What happens if you do the same thing to a CoreDNS pod by deleting its Pod object via kubectl delete pod?
A) The kube-apiserver if deleted will be removed form the control plane and will not be brought back up. It will require the manifest to reloaded on disk then 
the kind cluster has to be applied again to restart it. The CoreDNS if deleted will be brought up by kube-controller-manager then all DNS routes will be reassigned.

**CORRECTION — Q3 (score 2.5/5):**
No need to recreate the Kind cluster. kubelet **polls the static pod manifest directory** (`/etc/kubernetes/manifests/`) on a timer (~20s, `fileCheckFrequency`). It's a simple watch loop over a local directory, not an event push.
```
kubelet ──poll every ~20s──▶ /etc/kubernetes/manifests/*.yaml
   file deleted   ──▶ kubelet notices on next poll ──▶ stops + removes the static pod
   file restored  ──▶ kubelet notices on next poll ──▶ recreates the static pod
```
So: delete `kube-apiserver.yaml` from disk → apiserver pod is torn down within ~20s automatically. Put the file back → it comes back within ~20s automatically. No `kind create cluster`, no manual restart needed.

CoreDNS part: your answer is directionally okay but skips a step and overstates the DNS impact. It's the **ReplicaSet controller** that notices the missing pod and creates a replacement (kube-controller-manager runs that reconcile loop, but the object being reacted to is the ReplicaSet, not "DNS routes"). The **scheduler** still has to place the new pod. And the Service's ClusterIP never changes — only the pod IP inside the Endpoints object updates once the new CoreDNS pod is Ready. Nothing gets "reassigned" in a dramatic sense; it's just Endpoints catching up.

4. (Conceptual) Why does Kubernetes need static pods at all — what problem would exist if etcd and kube-apiserver were scheduled the normal way (as Deployments
managed by the API server)?
A) K8s requires static pods because this prevents worrying about making initial configurations for components that are common across any K8s cluster. The problem
that would exist is the initial start of the k8s cluster would then face the issue of not knowing where what resource is and who came first.

**CORRECTION — Q4 (score 3/5):**
Name the problem precisely: it's a **chicken-and-egg / bootstrap paradox**. The scheduler decides where a pod runs by reading cluster state from etcd, through the apiserver. But etcd and the apiserver *are themselves* pods that need to be scheduled and started. You can't use the scheduler to schedule the thing the scheduler depends on to exist first — it's circular.

Static pods break the cycle: kubelet starts them straight from a local manifest file on disk, with **zero dependency on scheduler or apiserver**. That's the entire reason they exist — to bootstrap the control plane before there's any control plane to talk to.
```
Normal pod:  apiserver ◀── scheduler ◀── kubelet  (needs apiserver+scheduler already running)
Static pod:  kubelet ── reads local manifest directly ── starts container  (needs nothing else running)
```

5. (Diagnostic) Two pods on two different worker nodes both get IP 10.244.1.5 and 10.244.2.5 respectively — different last octet ranges. What component assigned
those, and why aren't they node IPs?
A) CoreDNS assigned these two different IPs. These are IPs for resources in the cluster to identify the location of the worker nodes. They don't recieve the 
node IPs because these are the pod IPs to access the containers present on them.

**CORRECTION — Q5 (score 1.5/5) — FLAGGED WEAK AREA, recurring 2 sessions running:**
CoreDNS did NOT assign these IPs. **CoreDNS never allocates IPs — it only resolves names to IPs that something else already assigned.** Its entire job is DNS: name lookup, nothing about IP address management.

The component that actually assigns pod IPs is the **CNI plugin** — in your cluster, **kindnet**. Here's the mechanism that explains the different last-octet ranges you saw:
```
Cluster gets a big pod-CIDR block, e.g. 10.244.0.0/16
        │
        ├── node1 is allocated a slice:  10.244.1.0/24
        └── node2 is allocated a slice:  10.244.2.0/24

kindnet on node1 ──▶ assigns pod IPs from 10.244.1.0/24  (e.g. 10.244.1.5)
kindnet on node2 ──▶ assigns pod IPs from 10.244.2.0/24  (e.g. 10.244.2.5)
```
That's exactly why the last-octet *range* differed by node — each node owns a different /24 slice of the overall pod network, and the CNI plugin on that node hands out IPs from its own slice as pods are created. This is unrelated to node IPs (which come from the underlying infra/network, not Kubernetes) and unrelated to CoreDNS (which just answers "what IP is `nginx-svc`?" using whatever IP got written into Endpoints).

**Action item:** run `kubectl get pods -A -o wide` in your Kind cluster and look at the NODE and IP columns side by side — see the /24-per-node pattern with your own eyes before moving on. Same category of mistake as yesterday's "kindnet gives pods the node's IP" — get very clear on: **kindnet/CNI = IP allocation, CoreDNS = name resolution only.**

6. (Conceptual) In your nginx lab, you created a Service in front of the Deployment. What does the Service actually do at the networking level, and what breaks if
the Service's label selector doesn't match any pod labels (vs. what breaks if there's no Service at all)?
A) The service at the networking level assigns an IP for components in the cluster to reach it at. Then also assigns the label baseline-nginx to it.
This routes the label to the IP when called.

**CORRECTION — Q6 (score 1.5/5):**
Missing the actual mechanism — kube-proxy and Endpoints. A Service is a **stable virtual IP (ClusterIP)** that never has a real network interface of its own; traffic to it gets rewritten to a real pod IP by kube-proxy.
```
Client pod ──DNS lookup "nginx-svc"──▶ CoreDNS ──▶ returns ClusterIP (stable, virtual)
Client pod ──sends packet to ClusterIP:80──▶ kube-proxy's iptables/ipvs rules on the node
                                                  │ DNAT: rewrite destination to a real pod IP
                                                  ▼
                                           one of the actual nginx pods

Endpoints controller: continuously watches Service.selector vs Pod labels + readiness
                       → writes matching, Ready pod IPs into the Service's Endpoints object
                       → kube-proxy watches Endpoints and keeps its DNAT rules in sync
```
Now the two failure modes you were asked to compare:
- **Selector matches no pods:** The Service object still exists, the ClusterIP is still real, DNS still resolves it just fine. But Endpoints is empty — kube-proxy has no pod IP to rewrite to. The connection **hangs/times out** (or "connection refused" depending on setup). This is the nasty one: DNS looks healthy, only the last hop silently fails. This is your CLAUDE.md's Category 2 "service endpoint mismatch" scenario exactly.
- **No Service at all:** DNS lookup for `nginx-svc` fails immediately with **NXDOMAIN** — loud, fast, obvious. Much easier to diagnose than the selector-mismatch case.

7. (Tradeoff) Your OOMKill lab (Wednesday, upcoming) sets memory limit=64Mi, request=32Mi. Explain what request vs limit actually control differently — one governs
scheduling, the other governs runtime enforcement. What observably different failure would you get if you set limit=request=64Mi instead?
A) Request memory refers to handling the incoming requests to that pod. Memory limit overall assigns how much memory the pod can use to run.
If both are made the same then the failure we can run into is allowing more requests than it can handle.

**CORRECTION — Q7 (score 0.5/5) — you flagged you haven't done this lab yet, so full explanation from scratch:**

First, drop the word "requests" in the HTTP/network sense entirely — it has nothing to do with that here. "Request" and "limit" are both just **memory accounting numbers**, and they're read by two completely different consumers at two completely different times.

```
request=32Mi ──▶ read ONCE, at scheduling time, by kube-scheduler
                  "reserve 32Mi worth of capacity on whichever node I place this pod on"
                  purely a bin-packing promise — does NOT stop the container from using more

limit=64Mi   ──▶ enforced CONTINUOUSLY, at runtime, by the Linux kernel via cgroups
                  translates directly to: memory.max = 64Mi on this container's cgroup
                  if the container's actual RSS usage tries to cross 64Mi:
                      kernel cgroup OOM killer fires ──▶ SIGKILL the process ──▶ exit code 137
                      kubelet sees the exit code, marks the pod status "OOMKilled"
```

So: **request → scheduler's placement decision. limit → kernel's hard ceiling, enforced live, every allocation.** They answer two unrelated questions: "where should this run" vs "how much can it actually use once running."

Second effect of request/limit you didn't cover — **QoS class**, which matters under node-wide memory pressure (not just this one container OOMing):
```
requests == limits  (all resources)     ──▶ QoS class: Guaranteed  (last to be evicted)
requests <  limits                      ──▶ QoS class: Burstable   (evicted before Guaranteed)
no requests/limits set at all           ──▶ QoS class: BestEffort  (evicted first)
```
Your lab's current config (`request=32Mi`, `limit=64Mi`) is **Burstable**.

If you changed it to `limit=request=64Mi` (making it **Guaranteed**): the pod would NOT become immune to OOMKilling — if this *one* container's own usage exceeds 64Mi, the kernel still kills it, same as before. What actually changes:
1. **Scheduling** gets stricter — the scheduler now reserves the full 64Mi up front (no over-commit slack), so it can pack fewer such pods onto a node than it could when request was only 32Mi.
2. **Eviction priority under node-wide pressure changes** — if the *node itself* runs low on memory (from other pods, not this one), the kubelet evicts BestEffort pods first, then Burstable, and only kills Guaranteed pods last. So a Guaranteed pod survives node-level memory pressure situations that would have evicted it as Burstable — but it's still just as killable by its own container exceeding its own 64Mi limit.

**Why this matters for Wednesday's lab specifically:** when you watch your nginx pod get OOMKilled, you're watching the *kernel cgroup OOM killer* act on the `limit`, not anything scheduler-related. `kubectl describe pod` will show `Last State: Terminated, Reason: OOMKilled, Exit Code: 137` — that 137 = 128 + 9 (SIGKILL). Contrast this with CrashLoopBackOff from a bad command/missing ConfigMap, where the exit code is usually something else entirely (often 1 or 127) and there's no "OOMKilled" reason string — that's the actual distinguishing signal between the two failure types your CLAUDE.md tells you to tell apart.

8. (Diagnostic) kubelet reports node status to the API server via a heartbeat. If the control plane (API server) goes down entirely for 5 minutes but all worker
nodes and pods are healthy, what happens to already-running pods? What would NOT work during that window?
A) Rescheduling deletion and management of resources of those pods would not work since the control plane is down. Networking still should be possible i.e accessing those
nodes but any k8s functionality we would want to perform would not be possible.

**Q8 — 4.5/5, correct.** No correction needed — right reasoning on blast radius (existing pods + networking survive; anything needing the control plane, like scheduling/reconciliation/kubectl, does not).

---

## Summary — score 22/40 (55%)

**Solid:** static pod definition (Q2), control-plane-down blast radius (Q8).

**Recurring weak area — flag for revisit:** confusing CNI (kindnet) with CoreDNS. Yesterday: said kindnet assigns node IP to pods. Today: said CoreDNS assigns pod IPs. Locked-in rule: **CoreDNS = name resolution only, never allocates IPs. kindnet/CNI = the only thing that assigns pod IPs, from a per-node CIDR slice.** Action item before next session: run `kubectl get pods -A -o wide` and visually correlate NODE column to IP column.

**Needs work:**
- Watch/reconcile model — flip "X calls Y" to "X watches Y's state and reacts" (only kubectl→apiserver and kubelet→containerd are true direct calls).
- kubelet's static-pod-manifest poll loop (not "recreate the whole cluster").
- Service internals — kube-proxy + Endpoints object, not just "assigns an IP."
- Resource **request vs limit** — this was the biggest gap (0.5/5) and it's foundational for Wednesday's OOMKill lab. Full explanation written above — read it before Wednesday.
