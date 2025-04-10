#ifndef UTIL_HPP
#define UTIL_HPP

#include <functional>
#include <thread>
#include <vector>

double get_cpu_time_ms() {
	pid_t pid = getpid();
    std::ifstream stat_file("/proc/" + std::to_string(pid) + "/stat");
    if (!stat_file.is_open()) {
        throw std::runtime_error("Failed to open /proc/[pid]/stat");
    }

    std::string token;
    long utime_ticks = 0, stime_ticks = 0;
    for (int i = 1; i <= 15; ++i) {
        stat_file >> token;
        if (i == 14) utime_ticks = std::stol(token);
        if (i == 15) stime_ticks = std::stol(token);
    }

    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    double total_ms = (utime_ticks + stime_ticks) * 1000.0 / ticks_per_sec;
    return total_ms;
}

#endif // UTIL_HPP
