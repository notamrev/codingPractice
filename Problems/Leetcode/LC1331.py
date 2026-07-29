from typing import List

class Solution:
    def arrayRankTransform(self, arr: List[int]) -> List[int]:
        temp = sorted(set(arr))
        dict = {}
        for i in range(0,len(temp)):
            dict[temp[i]] = i+1
        ret = []
        for n in arr:
            ret.append(dict[n])
        return ret
