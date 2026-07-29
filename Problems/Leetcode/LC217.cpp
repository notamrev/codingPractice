#include<iostream>
#include <unordered_set>

using namespace std;

bool containsDuplicate(vector<int>& nums) {
    auto(num : nums){

    }
}

int main(){
    vector<int> nums = {1, 2, 3, 1};
    if(containsDuplicate(nums)){
        cout << "Contains duplicates" << endl;
    } else {
        cout << "No duplicates" << endl;
    }
    return 0;
}