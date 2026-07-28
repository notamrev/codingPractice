#include <iostream>
#include <map>

using namespace std;

class Solution {
public:
    string smallestPalindrome(string s) {
        map<char, int> letCount;
        for (char c : s) letCount[c]++;         
         
        string half, mid;
        for (auto& [ch, n] : letCount){
            half += string(n / 2, ch);
            if (n % 2) mid += ch;
        }

        return half + mid + string(half.rbegin(), half.rend());
    }
};

int main(){
    Solution sol;
    cout << sol.smallestPalindrome("avbba") << endl;
    return 0;
}