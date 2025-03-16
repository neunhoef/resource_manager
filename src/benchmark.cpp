#include "ResourceManager.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

// Standard read-write lock based resource manager for comparison
template <typename T>
class RWLockResourceManager {
private:
  std::unique_ptr<T> resource;
  mutable std::shared_mutex mutex;

public:
  explicit RWLockResourceManager(std::unique_ptr<T> initial_resource)
    : resource(std::move(initial_resource)) {}

  ~RWLockResourceManager() = default;

  // Delete copy/move constructors and assignment operators
  RWLockResourceManager(const RWLockResourceManager&) = delete;
  RWLockResourceManager& operator=(const RWLockResourceManager&) = delete;
  RWLockResourceManager(RWLockResourceManager&&) = delete;
  RWLockResourceManager& operator=(RWLockResourceManager&&) = delete;

  // Reader API: Get access to the resource
  template <typename F>
  auto read(F&& f) -> decltype(f(std::declval<const T&>())) {
    std::shared_lock<std::shared_mutex> lock(mutex);
    return f(*resource);
  }

  // Writer API: Update the resource
  std::pair<std::unique_ptr<T>, uint64_t> update(std::unique_ptr<T> new_resource) {
    std::unique_lock<std::shared_mutex> lock(mutex);
    std::unique_ptr<T> old_resource = std::move(resource);
    resource = std::move(new_resource);
    return std::make_pair(std::move(old_resource), 0); // No epoch needed
  }

  // No-op for compatibility with ResourceManager API
  void wait_reclaim(uint64_t) {}
};

// Command line argument parser
struct BenchmarkConfig {
  int reader_threads = 4;
  int duration_seconds = 10;
  int updates_per_second = 100;
  bool csv_output = false;
  std::string output_file = "benchmark_results.csv";
  bool run_both = true;  // Run both implementations by default

  static BenchmarkConfig parse_args(int argc, char *argv[]) {
    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];

      if (arg == "-r" || arg == "--readers") {
        if (++i < argc)
          config.reader_threads = std::stoi(argv[i]);
      } else if (arg == "-d" || arg == "--duration") {
        if (++i < argc)
          config.duration_seconds = std::stoi(argv[i]);
      } else if (arg == "-u" || arg == "--updates") {
        if (++i < argc)
          config.updates_per_second = std::stoi(argv[i]);
      } else if (arg == "--csv") {
        config.csv_output = true;
      } else if (arg == "-o" || arg == "--output") {
        if (++i < argc)
          config.output_file = argv[i];
      } else if (arg == "--epoch-only") {
        config.run_both = false;
      } else if (arg == "-h" || arg == "--help") {
        std::cout
            << "Usage: " << argv[0] << " [options]\n"
            << "Options:\n"
            << "  -r, --readers N    Number of reader threads (default: 4)\n"
            << "  -d, --duration N   Benchmark duration in seconds (default: "
               "10)\n"
            << "  -u, --updates N    Writer updates per second (default: 100)\n"
            << "  --csv              Output results in CSV format\n"
            << "  -o, --output FILE  Output file for CSV results (default: "
               "benchmark_results.csv)\n"
            << "  --epoch-only       Only run the epoch-based implementation\n"
            << "  -h, --help         Show this help message\n";
        exit(0);
      }
    }

    return config;
  }
};

// Statistics for a reader thread
class ReaderStats {
private:
  std::vector<double> latencies; // in nanoseconds
  std::mutex stats_mutex;
  int thread_id;
  uint64_t total_reads = 0;
  double duration_secs = 0.0;
  std::string implementation_name;

public:
  explicit ReaderStats(int id, const std::string& impl_name = "")
    : thread_id(id), implementation_name(impl_name) {
    // Pre-allocate space to avoid reallocations
    latencies.reserve(100000000);
  }

  void record_latency(double latency_us) {
    std::lock_guard<std::mutex> lock(stats_mutex);
    latencies.push_back(latency_us);
    total_reads++;
  }

  void set_duration(double secs) { duration_secs = secs; }

  int get_thread_id() const { return thread_id; }
  
  const std::string& get_implementation_name() const { return implementation_name; }

  uint64_t get_total_reads() const { return total_reads; }

  double get_reads_per_second() const {
    return (duration_secs > 0) ? (total_reads / duration_secs) : 0;
  }

  double get_average_latency() const {
    if (latencies.empty())
      return 0.0;
    return std::accumulate(latencies.begin(), latencies.end(), 0.0) /
           latencies.size();
  }

  double get_percentile(double percentile) const {
    if (latencies.empty())
      return 0.0;

    size_t idx = static_cast<size_t>(percentile * latencies.size());
    if (idx >= latencies.size())
      idx = latencies.size() - 1;

    return latencies[idx];
  }

  void sort_latencies() {
    std::cout << "Sorting latencies for " << implementation_name << " thread " << thread_id << "..." << std::endl;
    std::sort(latencies.begin(), latencies.end());
    std::cout << "Done." << std::endl;
  }

  void print_stats() const {
    std::cout << implementation_name << " Thread " << thread_id << ":" << std::endl;
    std::cout << "  Total reads: " << total_reads << std::endl;
    std::cout << "  Reads/sec: " << std::fixed << std::setprecision(2)
              << get_reads_per_second() << std::endl;
    std::cout << "  Median latency: " << std::fixed << std::setprecision(2)
              << get_percentile(0.5) << " ns" << std::endl;
    std::cout << "  Average latency: " << std::fixed << std::setprecision(2)
              << get_average_latency() << " ns" << std::endl;
    std::cout << "  90%ile latency: " << std::fixed << std::setprecision(2)
              << get_percentile(0.9) << " ns" << std::endl;
    std::cout << "  99%ile latency: " << std::fixed << std::setprecision(2)
              << get_percentile(0.99) << " ns" << std::endl;
    std::cout << "  99.9%ile latency: " << std::fixed << std::setprecision(2)
              << get_percentile(0.999) << " ns" << std::endl;
  }

  // For CSV output
  std::string get_csv_header() const {
    return "implementation,thread_id,total_reads,reads_per_sec,median_latency_ns,avg_latency_"
           "ns,"
           "p90_latency_ns,p99_latency_ns,p999_latency_ns";
  }

  std::string get_csv_row() const {
    std::stringstream ss;
    ss << implementation_name << "," << thread_id << "," << total_reads << "," << std::fixed
       << std::setprecision(2) << get_reads_per_second() << "," << std::fixed
       << std::setprecision(2) << get_percentile(0.5) << "," << std::fixed
       << std::setprecision(2) << get_average_latency() << "," << std::fixed
       << std::setprecision(2) << get_percentile(0.9) << "," << std::fixed
       << std::setprecision(2) << get_percentile(0.99) << "," << std::fixed
       << std::setprecision(2) << get_percentile(0.999);
    return ss.str();
  }
};

// Generic reader thread function template
template<typename ManagerType>
void reader_function(std::shared_ptr<ManagerType> manager,
                     ReaderStats &stats, std::atomic<bool> &should_stop) {
  auto start_time = std::chrono::steady_clock::now();

  while (!should_stop.load(std::memory_order_relaxed)) {
    // Measure latency of the read operation
    auto read_start = std::chrono::high_resolution_clock::now();

    manager->read([](const std::string &resource) {
      // Just read the resource, don't clone it to avoid measuring clone time
      volatile size_t len = resource.length();
      return len;
    });

    auto read_end = std::chrono::high_resolution_clock::now();
    auto latency =
        std::chrono::duration<double, std::nano>(read_end - read_start).count();

    stats.record_latency(latency);
  }

  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration<double>(end_time - start_time).count();
  stats.set_duration(duration);
}

// Generic writer thread function template
template<typename ManagerType>
void writer_function(std::shared_ptr<ManagerType> manager,
                     int updates_per_second, std::atomic<bool> &should_stop,
                     std::atomic<uint64_t> &update_counter) {
  // Calculate update interval
  auto update_interval =
      std::chrono::nanoseconds(1000000000 / updates_per_second);
  auto start_time = std::chrono::steady_clock::now();
  uint64_t counter = 0;

  while (!should_stop.load(std::memory_order_relaxed)) {
    // Calculate next update time
    auto next_update_time = start_time + counter * update_interval;
    auto now = std::chrono::steady_clock::now();

    if (now < next_update_time) {
      // Sleep until next update time
      std::this_thread::sleep_until(next_update_time);
    }

    // Create new resource and update
    auto new_value = std::make_unique<std::string>("Updated resource " +
                                                   std::to_string(counter));
    {
      auto [old_value, epoch] = manager->update(std::move(new_value));

      manager->wait_reclaim(epoch);
    }
    update_counter.fetch_add(1, std::memory_order_relaxed);

    counter++;
  }
}

// Function to run a single benchmark
template<typename ManagerType>
void run_benchmark(const BenchmarkConfig& config, const std::string& implementation_name) {
  std::cout << "\nRunning benchmark for " << implementation_name << ":" << std::endl;
  std::cout << "  Reader threads: " << config.reader_threads << std::endl;
  std::cout << "  Duration: " << config.duration_seconds << " seconds" << std::endl;
  std::cout << "  Writer updates: " << config.updates_per_second << " per second" << std::endl;

  // Create a resource manager with initial string
  auto manager = std::make_shared<ManagerType>(
      std::make_unique<std::string>("Initial resource"));

  // Create a stop flag to signal threads to stop
  std::atomic<bool> should_stop(false);

  // Create reader statistics objects
  std::vector<std::unique_ptr<ReaderStats>> all_stats;
  for (int i = 0; i < config.reader_threads; ++i) {
    all_stats.emplace_back(std::make_unique<ReaderStats>(i, implementation_name));
  }

  // Create and start reader threads
  std::vector<std::thread> reader_threads;
  for (int i = 0; i < config.reader_threads; ++i) {
    reader_threads.emplace_back(reader_function<ManagerType>, manager,
                                std::ref(*all_stats[i]), std::ref(should_stop));
  }

  // Create atomic counter for updates
  std::atomic<uint64_t> update_counter(0);

  // Create and start writer thread
  std::thread writer_thread(writer_function<ManagerType>, manager, config.updates_per_second,
                            std::ref(should_stop), std::ref(update_counter));

  // Run benchmark for specified duration
  std::cout << "Benchmark running for " << config.duration_seconds
            << " seconds..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(config.duration_seconds));

  // Signal threads to stop
  should_stop.store(true, std::memory_order_relaxed);

  // Wait for all threads to finish
  for (auto &thread : reader_threads) {
    thread.join();
  }
  writer_thread.join();

  // Collect and print results
  std::cout << "\n" << implementation_name << " Results:" << std::endl;
  std::cout << std::string(implementation_name.length() + 9, '=') << std::endl;

  uint64_t total_reads = 0;
  double total_duration = 0.0;

  for (const auto &stats : all_stats) {
    stats->sort_latencies();
    stats->print_stats();
    std::cout << std::endl;

    total_reads += stats->get_total_reads();
    total_duration +=
        stats->get_reads_per_second() > 0
            ? stats->get_total_reads() / stats->get_reads_per_second()
            : 0;
  }

  // Calculate aggregate statistics
  double avg_duration = total_duration / all_stats.size();
  double reads_per_second = total_reads / avg_duration;

  std::cout << implementation_name << " Aggregate Statistics:" << std::endl;
  std::cout << "  Total reads: " << total_reads << std::endl;
  std::cout << "  Total updates: " << update_counter.load() << std::endl;
  std::cout << "  Average duration: " << std::fixed << std::setprecision(2)
            << avg_duration << " seconds" << std::endl;
  std::cout << "  Total reads/sec: " << std::fixed << std::setprecision(2)
            << reads_per_second << std::endl;
  std::cout << "  Updates/sec: " << std::fixed << std::setprecision(2)
            << static_cast<double>(update_counter.load()) /
                   config.duration_seconds
            << std::endl;
            
  // Return the stats for CSV output if needed
  if (config.csv_output) {
    std::ofstream csv_file(config.output_file, 
                          implementation_name == "RWLock" ? std::ios::app : std::ios::out);

    if (csv_file.is_open()) {
      // Write header only for the first implementation
      if (implementation_name != "RWLock") {
        csv_file << all_stats[0]->get_csv_header() << std::endl;
      }

      // Write data rows
      for (const auto &stats : all_stats) {
        csv_file << stats->get_csv_row() << std::endl;
      }

      // Write aggregate row
      csv_file << implementation_name << ",aggregate," << total_reads << "," << std::fixed
               << std::setprecision(2) << reads_per_second << ",,,,,";
      csv_file << std::endl;

      std::cout << "CSV results for " << implementation_name << " written to " << config.output_file << std::endl;
    } else {
      std::cerr << "Error: Could not open file for CSV output." << std::endl;
    }
  }
}

int main(int argc, char *argv[]) {
  // Parse command line arguments
  BenchmarkConfig config = BenchmarkConfig::parse_args(argc, argv);

  std::cout << "Starting benchmark comparison" << std::endl;
  
  // Run the epoch-based ResourceManager benchmark
  run_benchmark<ResourceManager<std::string>>(config, "EpochBased");
  
  // Run the RWLock-based ResourceManager benchmark if requested
  if (config.run_both) {
    run_benchmark<RWLockResourceManager<std::string>>(config, "RWLock");
    
    // Print comparison summary
    std::cout << "\nComparison Summary:" << std::endl;
    std::cout << "===================" << std::endl;
    std::cout << "See detailed results above for performance metrics." << std::endl;
    std::cout << "The CSV output file contains data for both implementations for detailed analysis." << std::endl;
  }

  return 0;
}
