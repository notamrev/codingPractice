#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

using namespace std;

class Solution {
public:
    int minTimeToVisitAllPoints(vector<vector<int>>& points) {
        int res = 0;
        for (int i = 1; i < points.size(); i++) {
            int dx = abs(points[i][0] - points[i - 1][0]);
            int dy = abs(points[i][1] - points[i - 1][1]);
            res += max(dx, dy);
        }
        return res;
    }
};

int main() {
    vector<vector<int>> points = {{1, 1}, {3, 4}, {-1, 0}};
    cout << Solution().minTimeToVisitAllPoints(points) << endl;
    return 0;
}
