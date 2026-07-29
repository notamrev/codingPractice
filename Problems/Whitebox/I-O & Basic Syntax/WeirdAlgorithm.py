import time
import tracemalloc
from typing import List

class Solution:
    def sol(self, n: int) -> List[int]:
        temp = []
        while n != 1:
            if n % 2 == 0:
                n = n // 2
            else:
                n = (n * 3) + 1
            temp.append(n)
        return temp

tracemalloc.start()
start = time.perf_counter()

result = Solution().sol(3)

end = time.perf_counter()
current, peak = tracemalloc.get_traced_memory()
tracemalloc.stop()

print(f"Result: {result}")
print(f"Time: {end - start:.10f} sec")
print(f"Memory: {current} bytes (peak: {peak} bytes)")
