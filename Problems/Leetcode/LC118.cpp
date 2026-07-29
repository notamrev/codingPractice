#include<iostream>
#include<chrono>

using namespace std;

vector<vector<int>> pascalTriangle(int numRows){

}


int main(){
    int n;
    cin >> n;
    vector<vector<int>> result = pascalTriangle(n);
    for (const auto& row : result) {
        for (int num : row) {
            cout << num << " ";
        }
        cout << endl;
    }
    return 0;
}
