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

