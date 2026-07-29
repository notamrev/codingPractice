from collections import Counter

class Solution:
    def smallestPalindrome(self, s: str) -> str:
        letCount = Counter(s)
        mid = ''
        half = []

        for ch in sorted(letCount):
            n = letCount[ch]
            if n % 2: 
                mid = ch
            half.extend([ch] * (n // 2))

        half.sort()
        return ''.join(half) + mid + ''.join(reversed(half))