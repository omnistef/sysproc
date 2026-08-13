#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <cctype>
#include <vector>
#include <algorithm>
#include <csignal>
#include <thread>
#include <chrono>

using namespace std;

struct Process {
    int pid;
    string name;
    long ram_kb;
};

struct RamInfo {
    long total_kb;
    long available_kb;
    long used_kb;
    double used_percent;
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

RamInfo get_system_ram() {
    ifstream file("/proc/meminfo");
    string line;
    long total = 0;
    long available = 0;

    if (file.is_open()) {
        while (getline(file, line)) {
            stringstream ss(line);
            string key;
            long value;

            // Extragere simplă: primul cuvânt e cheia (ex: "MemTotal:"), al doilea e valoarea
            ss >> key >> value;

            if (key == "MemTotal:") {
                total = value;
            } 
            else if (key == "MemAvailable:") {
                available = value;
            }
        }
    }

    return {total, available};
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

string draw_progress_bar(double percent) {
    int filled = (percent / 100.0) * 15;
    string bar = "[";
    for (int i = 0; i < 15; i++) {
        if (i < filled) bar += "=";
        else bar += ".";
    }
    bar += "]";
    return bar;
}

int main() {
    ios::sync_with_stdio(false);

    while(true) {

        cout << "\033[2J\033[3J\033[H";

        RamInfo ram = get_system_ram();
        double total_gb = ram.total_kb / 1048576.0;
        double used_gb = ram.used_kb / 1048576.0;

        vector<Process> active_pids = all_pids();

        sort(active_pids.begin(), active_pids.end(), [](const Process& a, const Process& b) {
            return a.ram_kb > b.ram_kb;
        });

        cout << fixed << setprecision(2);
        cout << "======================================================\n";
        cout << " RAM Used: " << draw_progress_bar(ram.used_percent) << " "
             << used_gb << " GB / " << total_gb << " GB (" << ram.used_percent << "%)\n";
        cout << " Found " << active_pids.size() << " active processes on the current system.\n";
        cout << "======================================================\n\n";

        cout << "------------------------------------------------------\n";
        cout << left << setw(10) << "PID"
             << setw(18) << "RAM (MB)"
             << "PROCESS NAME\n";
        cout << "------------------------------------------------------\n";

        size_t limit = min((size_t)15, active_pids.size());

        for(size_t i = 0; i < limit; i++) {
            double ram_mb = active_pids[i].ram_kb / 1024.0;

            cout << left << setw(10) << active_pids[i].pid
                 << setw(18) << ram_mb
                 << active_pids[i].name << "\n";
        }

        cout << "------------------------------------------------------\n";
        cout.flush();

        this_thread::sleep_for(chrono::seconds(1));
    }

    return 0;
}
