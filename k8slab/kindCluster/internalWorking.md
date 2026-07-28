### Internal Working

*"When I ran `kubectl apply -f baseline-nginx.yaml`, what actually happened and what did each fucking thing do?*
#### Layer 1
Two kinds of machines in a cluster
| **CONTROL PLANE**                      | **WORKER NODE(S)**                         |
| -------------------------------------- | ------------------------------------------ |
| **The Brain**                          | **The Muscle**                             |
| Decides **what** should run            | Actually **runs** the containers           |
| Decides **where** workloads should run | Reports back the node and container health |


In a true deployment these are usually seperated out into two parts/two machines. In KIND they become a single entity
`control-plane` = lab simplification not how it works at scale.

#### Layer 2
What lives on the control plane - 4 Components
1. etcd - The database of the cluster - stroes every object and location associated with the cluster
2. kube-apiserver - The front door - EVERY request from kubectl, kubelet goes through the api-server. Read and writes to etcd.
3. kube-scheduler - The assigner - Watches for pods that no node assigned. Decides WHICH node they should run on.
4. kube-controller-manager - The enforcer. - Constantly compares what you asked for vs. what you have and takes action to match what you asked for.

#### Layer 3
What lives on every node (worker AND control-plane) - The agents
1. kubelet - The local agent, runs on every node. Talks to the API server asks what pods am I supposed to be runnign. Starts/stops containers to match.
2. kube-proxy - Local traffic cop - Set up for networking rules on the node so Service IPs actaully route to the right pod
3. kindnet (CNI) - Gives each pod its own IP address and lets pods talk to each other across nodes.

#### Layer 4 
One cluster wide service
1. CoreDNS - The Phonebook. Turns service names into service IPs.

#### Runtime Layer - Layer 5
1. This is containerd - the true executor of container images. Everything above manages and stores data. But to run actual pods you need containerd.

Kubelet - "I need a pod running: `nginx:1.27-alpine`"
containerD - Pulls the above mentioned image then executes it by talking to the linux kernel and doing the grunt work.

Kubelet behaves as the k8s aware manager. Talks to the API, decides _which_ pods should exist on this node, watches their health, reports status back.
ContainerD is just a slave straight up pulls and runs. Doesn't start stop based on its choices only based on what its told.


## Tracing `kubectl apply -f baseline-nginx.yaml`:
1. You call the command
2. `kube-apiserver` recieves the request: (authenticates you, validates the YAML, writes the deployment + service objects into etcd)
3. `kube-controller-manager` notices (it's constantly watching the API server for changes) - Deployment wants 1 replica and 0 exist.
4. `controller-manager` creates a Pod object via the API server - but no node is assigned.
5. `kube-scheduler` notices the unscheduled pod and picks a node. writes the decision back via the API server.
6. `kubelet` - On the node notices a pod is assigned to it. It asks containerD to pull the container and run with the configuration provided.
7. `kindnet` (CNI) - assigns the pod to its own IP address.
8. `seperately` kube-apiserver also created your Service object. CoreDNS picks it up and creates a DNS entry for "baseline-nginx"
kubeproxy also handles routing for this by setting up rules.

A rule of thumb nothing here talks to each other they all interact with the kube-apiserver, and everyone _watches_ it for changes relevant to them.


QnA
1. What diffrentiates etcd, kube-apiserver, kube-scheduler and kube-controller-manager over coredns and kube-proxy. In terms of how they get started?
A. etcd, kube-apiserver, kube-scheduler and kube-controller are part of the control plane which is Layer 1. These handle all i/o operations related to the actual working of internal services.
They start when the cluster is initialized. Whereas coredns and kube-proxy as setup when any worker node is initialized. These help the pods on the nodes get started and be accessible within the cluster.

[CORRECTED] The right idea (control plane vs worker-side) but missing the actual mechanism, which is what the question is really asking. The 4 control-plane components are **static pods**: kubelet reads their manifests directly off disk (`/etc/kubernetes/manifests/`) and starts them WITHOUT going through the API server at all — this solves the chicken-and-egg problem of "something has to start the API server before the API server exists to schedule things." coredns and kube-proxy are normal API-managed objects (Deployment / DaemonSet) — they get created the standard way: written to etcd via the API server, then scheduled and started by kubelet like any other pod. "Started via a manifest file kubelet reads directly" vs "started via the normal API server → scheduler → kubelet pipeline" is the actual differentiator.

2. Why can't kube-scheduler or kubelet talk to etcd directly?
A. Both kube-scheduler and kubelet cannot talk to etcd directly because the api server manages etcd solely it balances and handles all api requests. 

3. What's the actual job of kube-controller-manager, in one sentence?
A. This enforces deployments to match what you asked for vs. what exists on your cluster currently.

4. When you ran kubectl apply -f baseline-nginx.yaml, which component decided which node your pod would run on?
A. Kube-scheduler chooses where the pod should be assigned, until it notices an unassigned pod it has no node.

5. What's the difference between kubelet and containerd — who does what?
A. Kubelet and containerd. Kubelet understands the requirements but does not know how to pull and start containers by interacting with the linux kernel. This is done by containerd. It identifies and pulls images/starts containers.

6. In your podman ps -a output, etcd and kube-apiserver came back with a fresh AGE after the restart, but coredns and kube-proxy kept their old AGE with just a restart count bump. Why the difference?
A. This is because a fresh index of where what resources are created exist. The control plane and the cluster tooling does not need to be restarted for this.

[CORRECTED] This one wasn't just imprecise, the reasoning was backwards — needs a re-read. Direct consequence of Q1's answer: etcd/kube-apiserver are **static pods** — kubelet manages them directly and independently of the API server's object history. When kubelet restarted after the container came back up, it recreated the Pod OBJECT for these from scratch (it doesn't ask etcd "did this pod already exist?" — it just enforces the manifest file), giving them a fresh AGE. coredns/kube-proxy are normal API-managed objects living in etcd's persistent history — the Pod OBJECT itself was never destroyed, only the CONTAINER inside it died and got restarted, so the object's original creation timestamp (AGE) stayed the same and only RESTARTS incremented. Same static-pod-vs-API-managed-pod distinction from Q1, just observed as a side effect.

7. What role did kindnet play in getting your pod reachable?
A. Kindnet assigns the nodes IP to the pod and makes it reachable across the cluster.

[CORRECTED] Backwards — the pod does NOT get the node's IP. If it did, every pod on a node would collide on the same address and you couldn't run more than one pod per node. Kindnet (the CNI) gives each pod its OWN unique IP, separate from the node's IP, out of a cluster-wide pod IP range. That's what lets multiple pods live on the same node without conflicting, and lets pods reach each other by IP even across different nodes.

8. In one sentence: what's the core design pattern that ties all of Kubernetes' components together (the thing I said is "the one thing to really absorb")?
A. Nobody talks to each other. Every component/tooling talks strictly to the kube-apiserver which talks to etcd and does the managing watching for changes. 
