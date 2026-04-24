#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstdint>
#include <thread>
#include <atomic>
#include <x86intrin.h>
#include <new> // For hardware_destructive_interference_size()


std::atomic<bool> stop_noise(false);


// The "Noisy Neighbor" thread function
void noise_maker(long L3_size) {
    const size_t noise_size = 2 * L3_size * 1024; // Value should exceed L3
    std::vector<uint8_t> junk(noise_size, 0);
    while (!stop_noise) {
        for (size_t i = 0; i < noise_size; i += std::hardware_destructive_interference_size) { // Access every cache line
            ++junk[i];
        }
    }
}


struct CacheSpec {
    std::string label;
    long size_kb;
};


std::vector<CacheSpec> get_cpu_specs() {
    std::vector<CacheSpec> specs;
    for (int i = 0; i <= 4; ++i) {
        std::string path = "/sys/devices/system/cpu/cpu0/cache/index" + std::to_string(i) + "/";
        std::ifstream l_file(path + "level"), s_file(path + "size"), t_file(path + "type");
        std::string level, size_str, type;
        if (l_file >> level && s_file >> size_str && t_file >> type) {
            char unit = size_str.back();
            long size = std::stol(size_str.substr(0, size_str.size() - 1));
            if (unit == 'M') size *= 1024;
            specs.push_back({"L" + level + "-" + type, size});
        }
    }
    return specs;
}


uint64_t get_latency(size_t kb) {
    size_t bytes = kb * 1024;
    size_t elements = bytes / sizeof(void*);
    if (elements < 2) return 0;
    if (elements < 4096) elements = 4096; // Minimum size for meaningful stride

    std::vector<void*> buffer(elements);

    // Large prime stride (2053) to defeat modern complex prefetchers
    size_t stride = 2053;
    for (size_t i = 0; i < elements; ++i) {
        buffer[i] = (void*)&buffer[(i + stride) % elements];
    }

    void* ptr = buffer.data();
    unsigned int ui;

    // Longer warmup to ensure TLB is primed
    for (int i = 0; i < 2000000; ++i) ptr = *(void**)ptr; 

    uint64_t start = __rdtscp(&ui);
    for (int i = 0; i < 10000000; ++i) ptr = *(void**)ptr;
    uint64_t end = __rdtscp(&ui);

    if (ptr == nullptr) std::cout << " ";
    return (end - start) / 10000000;
}


int main(int argc, char** argv) {
    bool enable_noise = (argc > 1 && std::string(argv[1]) == "--noise");

    auto specs = get_cpu_specs();
    std::ofstream spec_out("cpu_specs.csv");
    spec_out << "label,size_kb\n";
    long L3_size = 0;
    for (auto& s : specs) {
        spec_out << s.label << "," << s.size_kb << "\n";
        if (s.label.find("L3") != std::string::npos) {
            L3_size_kb = s.size_kb;
        }
    }
    if (L3_size_kb == 0) {
        std::cout << "[!] L3 cache size is unclear. Can't detect Noisy Neighbors. Stop." << std::endl;
        return 0;
    }

    std::thread n_thread;
    if (enable_noise) {
        std::cout << "[!] Starting NOISY NEIGHBOR thread..." << std::endl;
        n_thread = std::thread(noise_maker, L3_size);
    }

    std::ofstream out("latency_data.csv");
    out << "size_kb,latency\n";
    for (size_t kb = 8; kb <= 10 * L3_size_kb; kb = static_cast<size_t>(kb * 1.15) + 1) {
        uint64_t lat = get_latency(kb);
        out << kb << "," << lat << "\n";
        std::cout << "KB: " << kb << "\tLat: " << lat << std::endl;
    }

    if (enable_noise) {
        stop_noise = true;
        n_thread.join();
    }
    return 0;
}
