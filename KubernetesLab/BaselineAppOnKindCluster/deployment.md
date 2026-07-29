### Basic Notes and understanding as I go step by step
1. So first I have to get Podman up and running a lightweight tooling compared to docker
2. Then get Kind running but then change the env to use podman and not docker
3. Get kubectl running 
4. Deploy the baseline app. (Idk what baseline app means let me check)

``` 
Quick terminology notes

So `Docker` refers to the container/the app you want to run. This is a package of the dependancies
and everything else your app requires

`Kubernetes` refers to the manager of this system. 
`KIND` Is Kubernetes in Docker. You run multiple kubernetes tools in docker image formats to test out locally.
Essentially a lightweight K8s cluster to learn/test/verify etc.
```

### Podman Testing
So now that I am using Podman instead of docker 
the working of podman is 
``` > podman machine init ```
``` > podman machine start ```
``` > podman ps ```

Notes:
1. Had to set podman to rootful to get access to ports before 1024
- This also enables something to do with cgroup availability should read into it
2. Then added access to use podman machine as the experimental provider for KIND 
3. Verified all 3 installations
4. When you run ``` > kind create cluster ``` a default cluster with the name as KIND is created in the kind-kind context
5. To add with your own name use the --name flag
6. I set the context using the kubectl command. This was done with doing 
`kubectl cluster-info --context kind-sre-lab`
7. Get the pods doing kubectl get nodes
8. Now creating a yaml file to test with.
9. To create a new app you do
```
kubectl apply -f baseline-nginx.yaml
kubectl get pods
kubectl get deployements
kubectl get svc
```
10. To now test the yaml brought up the pod you want you can verify with this:
```
kubectl run curl-test --image=curlimages/curl:latest --rm -it --restart=Never -- curl -s http://baseline-nginx
```
-> Understanding the command this will: create a temporary throw away pod. The pod will make the curl call, never restart then delete itself after running the specified command.


Questions
1. Why does the curl resolve `http://baseline-nginx` 
A. This is because the pod has a resolv.conf for DNS that points directly to CoreDNS of the cluster
How this works: `curl-test` is a temporary pod in the same cluster. `baseline-nginx` is NOT an image name — it's the Service name (from `metadata.name` in the YAML). I'm not even reaching the nginx pod directly by name; I'm reaching the Service, which then forwards to whichever pod is currently backing it. The curl command triggers a DNS lookup for that Service name, CoreDNS resolves it to the Service's ClusterIP, and kube-proxy forwards traffic from there to a real pod IP. This way the caller never needs to know or track the pod's actual (and constantly changing) IP.

---

### Recap of Today (2026-07-26, Week 1 Monday — K8s lab: deploy baseline app on Kind)

- Installed Podman (lightweight alternative to Docker Desktop), set to rootful mode (needed for Kind compatibility / cgroup access)
- Installed Kind + kubectl, pointed Kind at Podman via `KIND_EXPERIMENTAL_PROVIDER=podman`
- Created Kind cluster named `sre-lab` — control plane came up `Ready`
- Wrote `baseline-nginx.yaml` by hand: a Deployment (nginx, 1 replica, tight memory limits — 64Mi limit / 32Mi request, deliberately set for Wednesday's OOMKill lab) + a Service (ClusterIP)
- Applied it — pod `Running`, 0 restarts, deployment `1/1 AVAILABLE`, Service got a ClusterIP
- Proved the full traffic path end-to-end: spun up a throwaway `curl-test` pod, hit `http://baseline-nginx` by Service DNS name, got back real nginx HTML
- Learned the DNS resolution chain: pod's `/etc/resolv.conf` (auto-configured by kubelet) points at CoreDNS → CoreDNS resolves Service name to ClusterIP → kube-proxy forwards to a real pod IP behind the Service
- Key correction: `baseline-nginx` is the **Service name**, not an image name or the pod's own address — the Service is a stable indirection layer in front of pods, which are disposable and get new IPs whenever recreated

**Next:** Wednesday — break this same `baseline-nginx` pod with OOMKill, observe CrashLoopBackOff vs OOMKilled distinction. VM/paging deferred to Thursday for a fresh start.







