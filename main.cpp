#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <cctype>
#include <vector>
#include <algorithm>
#include <csignal>

using namespace std;

struct Process {
    int pid;
    string name;
    long ram_kb;
};

bool number(const string& s) {
    if(s.empty())
        return false;
    for(size_t i = 0; i < s.size(); i++) {
        if(!isdigit(s[i])) {
            return false;
        }
    }
    return true;
}

vector<int> all_pids() {
    vector<int> pids;
    for(const auto& entry : filesystem::directory_iterator("/proc")) {
        if(entry.is_directory()) {
            string folder = entry.path().filename().string();
            if(number(folder)) {
                pids.push_back(stoi(folder));
            }
        }
    }
    return pids;
}




int main() {
    ios::sync_with_stdio(false);
    cout << "scanning /proc for PIDS...\n\n";
    vector<int> active_pids = all_pids();
    cout << "Found " << active_pids.size() << " active processes on the current system.\n\n";
    return 0;

}
