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
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
using namespace std;

//terminal setting
void set_terminal_raw_mode(bool enable) {
    static struct termios oldt, newt;

    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

bool kbhit() {
    timeval tv = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}
//terminal setting
    
//structs
struct Process {
    int pid;
    string name;
    long ram_kb;
};

struct RamInfo {
    long total_kb;
    long available_kb;
};
//structs

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

RamInfo system_ram() {
    ifstream file("/proc/meminfo");
    string line;
    long total = 0;
    long available = 0;

    if(file.is_open()) {
        while(getline(file, line)) {
            stringstream ss(line);
            string key;
            long value;

            ss >> key >> value;

            if(key == "MemTotal:") {
                total = value;
            } else if(key == "MemAvailable:") {
                available = value;                
            }

        }
    }
    return {total, available};
}

void read_cpu(long &total_out, long &idle_out) {
    ifstream file("/proc/stat");
    string line;

    long user, nice, system, idle_val, iowait, irq, softirq, steal;

    file >> line >> user >> nice >> system >> idle_val >> iowait >> irq >> softirq >> steal;
    
    idle_out = idle_val + iowait;
    total_out = user + nice + system + idle_out + iowait + irq + softirq + steal;

}

double calculate_cpu() {
    static long prev_total = 0;
    static long prev_idle = 0;
    
    long curr_total = 0;
    long curr_idle = 0;

    read_cpu(curr_total, curr_idle);

    if (prev_total == 0) {
        prev_total = curr_total;
        prev_idle = curr_idle;
        return 0.0;
    }

    long delta_total = curr_total - prev_total;
    long delta_idle  = curr_idle - prev_idle;

    prev_total = curr_total;
    prev_idle = curr_idle;

    if (delta_total == 0) return 0.0;

    return (double)(delta_total - delta_idle) / delta_total * 100.0;    

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

string progress_bar(double percent, int width = 15) {
    int filled = (percent / 100.0) * width;

    string bar = "[";
    for(int i = 0; i < width; i++) {
        if(i < filled) {
            bar += "=";
        } else {
            bar += ".";
        }
    }
    bar += "]";
    return bar;
}

int main() {
    ios::sync_with_stdio(false);

    set_terminal_raw_mode(true);

    size_t scroll_offset = 0;    
    const size_t max_visible = 15; 

    bool running = true;
    while(running) {

        if (kbhit()) {
            char c = getchar();
            if (c == '\033') {  
                getchar();     
                char arrow = getchar();
                
                if (arrow == 'B') { 
                    scroll_offset++;
                } else if (arrow == 'A') { 
                    if (scroll_offset > 0) {
                        scroll_offset--;
                    }
                }
            } else if (c == 'q' || c == 'Q') { 
                running = false;
            }
        }
        
        double cpu_usage = calculate_cpu();

        cout << "\033[2J\033[3J\033[H";
        
        RamInfo ram = system_ram();
        long used_kb = ram.total_kb - ram.available_kb;
        double used_gb = used_kb / 1048576.0;
        double total_gb = ram.total_kb / 1048576.0;
        double used_percentage = ((double)used_kb / ram.total_kb) * 100.0;

        vector<Process> active_pids = all_pids();

        sort(active_pids.begin(), active_pids.end(), [](const Process& a, const Process& b) {
            return a.ram_kb > b.ram_kb;
        });

        if (scroll_offset >= active_pids.size()) {
            scroll_offset = active_pids.size() > 0 ? active_pids.size() - 1 : 0;
        }

        cout << fixed << setprecision(2);
        cout << "======================================================\n";
        cout << "CPU Usage : " << progress_bar(cpu_usage) << " " << cpu_usage << "%\n"; 
        cout << "RAM Usage : " << progress_bar(used_percentage) << " " << used_gb << " GB / " << total_gb << " GB (" << used_percentage << "%)\n"; 
        cout << "Found " << active_pids.size() << " active processes. [Use UP/DOWN arrows, 'q' to quit]\n";
        cout << "======================================================\n\n";
        cout << "Your active processes:\n";
        cout << "------------------------------------------------------\n";
        cout << left << setw(10) << "PID"
             << setw(18) << "RAM (MB)"
             << "PROCESS NAME\n";
        cout << "------------------------------------------------------\n";

        size_t end_index = min(scroll_offset + max_visible, active_pids.size());
        
        for(size_t i = scroll_offset; i < end_index; i++) {
            double ram_mb = active_pids[i].ram_kb / 1024.0;
            cout << left << setw(10) << active_pids[i].pid 
                 << setw(18) << ram_mb 
                 << active_pids[i].name << "\n";
        }

        cout << "------------------------------------------------------\n";
        cout << " Displaying " << (active_pids.empty() ? 0 : scroll_offset + 1) 
             << "-" << end_index << " of " << active_pids.size() << "\n";
        cout.flush();

        this_thread::sleep_for(chrono::milliseconds(50));
    }

    set_terminal_raw_mode(false);
    return 0;
}
