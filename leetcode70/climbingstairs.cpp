#include<iostream>
#include<chrono>
#include<unordered_map>

using namespace std;
using namespace std::chrono;

int fibonacciMethod(int n){
    if (n<=1) return 1;
    return fibonacciMethod(n-1) + fibonacciMethod(n-2);
}

int memoizationMethod(int n, unordered_map<int, int> &memo){
    if (n<=1) return 1;
    if (memo.find(n) == memo.end())
        memo[n] = memoizationMethod(n-1, memo) + memoizationMethod(n-2, memo);
    return memo[n];
}

int tabulationMethod(int n){
    if (n<=1) return 1;
    int dp[n+1];
    dp[0] = 1;
    dp[1] = 1;
    for (int i=2; i<=n; i++)
        dp[i] = dp[i-1] + dp[i-2];
    return dp[n];
}

int storageMethod(int n){
    if (n<=1) return 1;
    int a = 1, b = 1, c;
    for (int i=2; i<=n; i++){
        c = a + b;
        a = b;
        b = c;
    }
    return b;
}

int main() {
    int n;
    
    // Test 1
    cin >> n;
    unordered_map<int, int> memo1;
    auto start1 = high_resolution_clock::now();
    int fib_result = fibonacciMethod(n);
    auto stop1 = high_resolution_clock::now();
    auto duration1 = duration_cast<nanoseconds>(stop1 - start1);
    
    auto start2 = high_resolution_clock::now();
    int memo_result = memoizationMethod(n, memo1);
    auto stop2 = high_resolution_clock::now();
    auto duration2 = duration_cast<nanoseconds>(stop2 - start2);

    auto start3 = high_resolution_clock::now();
    int tabulation_result = tabulationMethod(n);
    auto stop3 = high_resolution_clock::now();
    auto duration3 = duration_cast<nanoseconds>(stop3 - start3);


    cout << "fibonacci: " << fib_result << " Time: " << duration1.count() << " ns\n";
    cout << "memoization: " << memo_result << " Time: " << duration2.count() << " ns\n";
    cout << "tabulation: " << tabulation_result << " Time: " << duration3.count() << " ns\n";
    
    return 0;
}
