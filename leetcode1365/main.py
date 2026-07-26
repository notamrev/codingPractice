class Solution:
    def smallerNumbersThanCurrent(self, nums: List[int]) -> List[int]:
        # [8 1 2 2 3]
        # [1 2 2 3 8]
        # 0, 1 -> 1,0
        # 1, 2 -> 2,1
        # 2, 2 -> 2,1 (update skip)
        # 3, 3 -> 3,4 
        # 4, 8 -> 8,3
        s = sorted(nums)
        d = {}
        for i, num in enumerate(s):
            if num not in d:
                d[num] = i

        t = []
        for i in nums:
            t.append(d[i])
        
        return t
