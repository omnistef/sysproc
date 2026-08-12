#include <sstream>
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

string process_name(int pid) {
     string path = "/proc/" + to_string(pid) + "/comm";
     ifstream file(path);
     string name = "N/A";
     if(file.is_open()) {
         getline(file, name);
     }
     return name;
}

long process_ram(int pid) {
    string path = "/proc/" + to_string(pid) + "/status";
    ifstream file(path);
    string line;

    if(file.is_open()) {
        while(getline(file, line)) {
            if(line.rfind("VmRSS:" , 0) == 0) {
                stringstream ss(line);
                string label;
                long ram_kb = 0;
                ss >> label >> ram_kb;
                return ram_kb;
            }
        }
    }
    return 0;
}


vector<Process> all_pids() {
    vector<Process> pids;
    for(const auto& entry : filesystem::directory_iterator("/proc")) {
        if(entry.is_directory()) {
            string folder = entry.path().filename().string();
            if(number(folder)) {
                int pid = stoi(folder);
                string name = process_name(pid);
                long ram = process_ram(pid);
                pids.push_back({pid, name, ram});
            }
        }
    }
    return pids;
}


int main() {
    ios::sync_with_stdio(false);
    cout << "scanning /proc for PIDS...\n";
    vector<Process> active_pids = all_pids();
    sort(active_pids.begin(), active_pids.end(), [](const Process& a, const Process& b) {
        return a.ram_kb > b.ram_kb;
    });
    cout << "Found " << active_pids.size() << " active processes on the current system.\n\n";

    cout << "------------------------------------------------------\n";
    cout << "PID\t\tRAM (MB)\t\tPROCESS NAME\n";
    cout << "------------------------------------------------------\n";

    cout << fixed << setprecision(2);

    for(size_t i = 0; i < active_pids.size(); i++) {
        double ram_mb = active_pids[i].ram_kb / 1024.0;

        cout << active_pids[i].pid << "\t\t" << ram_mb << "MB\t\t" << active_pids[i].name << "\n";
    }

    return 0;

}
