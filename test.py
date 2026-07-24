import time
import tracemalloc

class Solution:
    def sol(self, n: int, A: int) -> int:
        if n % A == 0:
            return n // A
        return -1

tracemalloc.start()
start = time.perf_counter()

result = Solution().sol(400, 400)

end = time.perf_counter()
current, peak = tracemalloc.get_traced_memory()
tracemalloc.stop()

print(f"Result: {result}")
print(f"Time: {end - start:.10f} sec")
print(f"Memory: {current} bytes (peak: {peak} bytes)")

