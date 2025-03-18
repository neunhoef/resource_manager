#include "AtomicList.h"
#include "BoundedList2.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <jemalloc/jemalloc.h>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace arangodb {

// Simple payload class with two strings and a memoryUsage method
class Payload {
private:
  int64_t a;
  int64_t b;

public:
  Payload(uint64_t a, uint64_t b) : a(a), b(b) {}

  // Copy constructor
  Payload(const Payload &other) = default;

  // Move constructor
  Payload(Payload &&other) noexcept : a(other.a), b(other.b) {}

  // Memory usage estimation
  size_t memoryUsage() const { return sizeof(Payload); }
};

// Command line argument parser
struct BenchmarkConfig {
  int writer_threads = 4;
  int duration_seconds = 10;
  size_t memory_threshold = 1024 * 1024; // 1MB
  size_t max_history = 10;
  bool csv_output = false;
  std::string output_file = "bounded_list_benchmark.csv";

  static BenchmarkConfig parse_args(int argc, char *argv[]) {
    BenchmarkConfig config;

    for (int i = 1; i < argc; ++i) {
      std::string arg = argv[i];

      if (arg == "-w" || arg == "--writers") {
        if (++i < argc)
          config.writer_threads = std::stoi(argv[i]);
      } else if (arg == "-d" || arg == "--duration") {
        if (++i < argc)
          config.duration_seconds = std::stoi(argv[i]);
      } else if (arg == "-m" || arg == "--memory") {
        if (++i < argc)
          config.memory_threshold = std::stoul(argv[i]);
      } else if (arg == "-h" || arg == "--history") {
        if (++i < argc)
          config.max_history = std::stoul(argv[i]);
      } else if (arg == "--csv") {
        config.csv_output = true;
      } else if (arg == "-o" || arg == "--output") {
        if (++i < argc)
          config.output_file = argv[i];
      } else if (arg == "--help") {
        std::cout
            << "Usage: " << argv[0] << " [options]\n"
            << "Options:\n"
            << "  -w, --writers N    Number of writer threads (default: 4)\n"
            << "  -d, --duration N   Benchmark duration in seconds (default: "
               "10)\n"
            << "  -m, --memory N     Memory threshold in bytes (default: "
               "1048576)\n"
            << "  -h, --history N    Max history size (default: 10)\n"
            << "  --csv              Output results in CSV format\n"
            << "  -o, --output FILE  Output file for CSV results (default: "
               "bounded_list_benchmark.csv)\n"
            << "  --help             Show this help message\n";
        exit(0);
      }
    }

    return config;
  }
};

// Statistics for a writer thread
class WriterStats {
private:
  std::vector<double> latencies; // in nanoseconds
  std::unique_ptr<std::mutex> stats_mutex;
  int thread_id;
  uint64_t total_writes = 0;
  double duration_secs = 0.0;
  std::string implementation_name;

public:
  explicit WriterStats(int id, const std::string &impl_name = "")
      : stats_mutex(std::make_unique<std::mutex>()), thread_id(id),
        implementation_name(impl_name) {
    // Pre-allocate space to avoid reallocations
    latencies.reserve(20000000);
  }

  // Delete copy constructor
  WriterStats(const WriterStats &) = delete;
  WriterStats &operator=(const WriterStats &) = delete;

  // Move constructor
  WriterStats(WriterStats &&other) noexcept
      : latencies(std::move(other.latencies)),
        stats_mutex(std::move(other.stats_mutex)), thread_id(other.thread_id),
        total_writes(other.total_writes), duration_secs(other.duration_secs),
        implementation_name(std::move(other.implementation_name)) {}

  // Move assignment
  WriterStats &operator=(WriterStats &&other) noexcept {
    if (this != &other) {
      latencies = std::move(other.latencies);
      stats_mutex = std::move(other.stats_mutex);
      thread_id = other.thread_id;
      total_writes = other.total_writes;
      duration_secs = other.duration_secs;
      implementation_name = std::move(other.implementation_name);
    }
    return *this;
  }

  void record_latency(double latency_ns) {
    std::lock_guard<std::mutex> lock(*stats_mutex);
    latencies.push_back(latency_ns);
    total_writes++;
  }

  void set_duration(double secs) { duration_secs = secs; }

  int get_thread_id() const { return thread_id; }

  const std::string &get_implementation_name() const {
    return implementation_name;
  }

  uint64_t get_total_writes() const { return total_writes; }

  double get_writes_per_second() const {
    return (duration_secs > 0) ? (total_writes / duration_secs) : 0;
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
    std::cout << "Sorting latencies for " << implementation_name << " thread "
              << thread_id << "..." << std::endl;
    std::sort(latencies.begin(), latencies.end());
    std::cout << "Done." << std::endl;
  }

  void print_stats() const {
    std::cout << implementation_name << " Thread " << thread_id << ":"
              << std::endl;
    std::cout << "  Total writes: " << total_writes << std::endl;
    std::cout << "  Writes/sec: " << std::fixed << std::setprecision(2)
              << get_writes_per_second() << std::endl;
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
    return "implementation,thread_id,total_writes,writes_per_sec,"
           "median_latency_ns,avg_latency_ns,"
           "p90_latency_ns,p99_latency_ns,p999_latency_ns";
  }

  std::string get_csv_row() const {
    std::stringstream ss;
    ss << implementation_name << "," << thread_id << "," << total_writes << ","
       << std::fixed << std::setprecision(2) << get_writes_per_second() << ","
       << std::fixed << std::setprecision(2) << get_percentile(0.5) << ","
       << std::fixed << std::setprecision(2) << get_average_latency() << ","
       << std::fixed << std::setprecision(2) << get_percentile(0.9) << ","
       << std::fixed << std::setprecision(2) << get_percentile(0.99) << ","
       << std::fixed << std::setprecision(2) << get_percentile(0.999);
    return ss.str();
  }
};

// Writer function for BoundedList
template <typename ListType>
void writer_function(std::shared_ptr<ListType> list, WriterStats &stats,
                     std::atomic<bool> &should_stop, int thread_id) {
  uint64_t counter = 0;
  std::string prefix = "Thread-" + std::to_string(thread_id) + "-Item-";

  while (!should_stop.load(std::memory_order_relaxed)) {
    Payload payload(counter, 2 * counter);

    // Measure prepend latency
    auto start = std::chrono::high_resolution_clock::now();
    list->prepend(std::move(payload));
    auto end = std::chrono::high_resolution_clock::now();

    double latency_ns =
        std::chrono::duration<double, std::nano>(end - start).count();
    stats.record_latency(latency_ns);
  }
}

// Run benchmark for a specific list implementation
template <typename ListType>
void run_benchmark(const BenchmarkConfig &config,
                   const std::string &implementation_name) {
  std::cout << "Running benchmark for " << implementation_name << std::endl;
  std::cout << "  Writer threads: " << config.writer_threads << std::endl;
  std::cout << "  Duration: " << config.duration_seconds << " seconds"
            << std::endl;
  std::cout << "  Memory threshold: " << config.memory_threshold << " bytes"
            << std::endl;
  std::cout << "  Max history: " << config.max_history << std::endl;

  // Create the list
  auto list =
      std::make_shared<ListType>(config.memory_threshold, config.max_history);

  // Create writer threads and stats
  std::vector<std::thread> writer_threads;
  std::vector<WriterStats> writer_stats;
  writer_stats.reserve(config.writer_threads);

  std::atomic<bool> should_stop{false};

  for (int i = 0; i < config.writer_threads; ++i) {
    writer_stats.push_back(WriterStats(i, implementation_name));
  }

  auto start_time = std::chrono::high_resolution_clock::now();

  // Start writer threads
  for (int i = 0; i < config.writer_threads; ++i) {
    writer_threads.emplace_back(writer_function<ListType>, list,
                                std::ref(writer_stats[i]),
                                std::ref(should_stop), i);
  }

  // Sleep for the specified duration
  std::this_thread::sleep_for(std::chrono::seconds(config.duration_seconds));

  // Signal threads to stop
  should_stop.store(true, std::memory_order_relaxed);

  // Wait for all threads to finish
  for (auto &thread : writer_threads) {
    thread.join();
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  double duration_secs =
      std::chrono::duration<double>(end_time - start_time).count();

  // Set duration and sort latencies for stats
  uint64_t total_writes = 0;
  for (auto &stats : writer_stats) {
    stats.set_duration(duration_secs);
    stats.sort_latencies();
    total_writes += stats.get_total_writes();
  }

  // Print results
  std::cout << "\nResults for " << implementation_name << ":\n" << std::endl;

  // Print individual thread stats
  for (const auto &stats : writer_stats) {
    stats.print_stats();
  }

  std::cout << "\nResults for " << implementation_name << ":" << std::endl;
  std::cout << "  Total duration: " << std::fixed << std::setprecision(2)
            << duration_secs << " seconds" << std::endl;
  std::cout << "  Total writes: " << total_writes << std::endl;
  std::cout << "  Writes/sec: " << std::fixed << std::setprecision(2)
            << (total_writes / duration_secs) << std::endl;

  // Calculate and print aggregate stats
  std::vector<double> all_latencies;
  for (const auto &stats : writer_stats) {
    // We need to get all latencies from each thread and combine them
    // This is a simplification - in a real implementation we would
    // merge the sorted latency vectors more efficiently
    for (size_t i = 0; i < stats.get_total_writes(); ++i) {
      double percentile = static_cast<double>(i) / stats.get_total_writes();
      all_latencies.push_back(stats.get_percentile(percentile));
    }
  }

  std::sort(all_latencies.begin(), all_latencies.end());

  double avg_latency = 0;
  if (!all_latencies.empty()) {
    avg_latency =
        std::accumulate(all_latencies.begin(), all_latencies.end(), 0.0) /
        all_latencies.size();
  }

  std::cout << "\nAggregate stats for " << implementation_name << ":"
            << std::endl;
  std::cout << "  Median latency: " << std::fixed << std::setprecision(2)
            << (all_latencies.empty() ? 0.0
                                      : all_latencies[all_latencies.size() / 2])
            << " ns" << std::endl;
  std::cout << "  Average latency: " << std::fixed << std::setprecision(2)
            << avg_latency << " ns" << std::endl;
  std::cout
      << "  90%ile latency: " << std::fixed << std::setprecision(2)
      << (all_latencies.empty()
              ? 0.0
              : all_latencies[static_cast<size_t>(0.9 * all_latencies.size())])
      << " ns" << std::endl;
  std::cout
      << "  99%ile latency: " << std::fixed << std::setprecision(2)
      << (all_latencies.empty()
              ? 0.0
              : all_latencies[static_cast<size_t>(0.99 * all_latencies.size())])
      << " ns" << std::endl;
  std::cout << "  99.9%ile latency: " << std::fixed << std::setprecision(2)
            << (all_latencies.empty() ? 0.0
                                      : all_latencies[static_cast<size_t>(
                                            0.999 * all_latencies.size())])
            << " ns" << std::endl;

  // Write CSV output if requested
  if (config.csv_output) {
    std::ofstream csv_file(config.output_file, std::ios::app | std::ios::out);

    // Write header if file is empty
    csv_file.seekp(0, std::ios::end);
    if (csv_file.tellp() == 0) {
      csv_file << writer_stats[0].get_csv_header() << ",aggregate\n";
    }

    // Write individual thread stats
    for (const auto &stats : writer_stats) {
      csv_file << stats.get_csv_row() << ",thread\n";
    }

    // Write aggregate stats
    csv_file << implementation_name << ",aggregate," << total_writes << ","
             << std::fixed << std::setprecision(2)
             << (total_writes / duration_secs) << "," << std::fixed
             << std::setprecision(2)
             << (all_latencies.empty()
                     ? 0.0
                     : all_latencies[all_latencies.size() / 2])
             << "," << std::fixed << std::setprecision(2) << avg_latency << ","
             << std::fixed << std::setprecision(2)
             << (all_latencies.empty() ? 0.0
                                       : all_latencies[static_cast<size_t>(
                                             0.9 * all_latencies.size())])
             << "," << std::fixed << std::setprecision(2)
             << (all_latencies.empty() ? 0.0
                                       : all_latencies[static_cast<size_t>(
                                             0.99 * all_latencies.size())])
             << "," << std::fixed << std::setprecision(2)
             << (all_latencies.empty() ? 0.0
                                       : all_latencies[static_cast<size_t>(
                                             0.999 * all_latencies.size())])
             << ",aggregate\n";
  }
}

} // namespace arangodb

int main(int argc, char *argv[]) {
  using namespace arangodb;

  auto config = BenchmarkConfig::parse_args(argc, argv);

  std::cout << "=== BoundedList vs BoundedList2 Benchmark ===" << std::endl;

  // Run benchmark for BoundedList
  run_benchmark<BoundedList<Payload>>(config, "BoundedList");

  std::cout << "\n\n";
  // malloc_stats_print(nullptr, nullptr, nullptr);

  // Run benchmark for BoundedList2
  run_benchmark<BoundedList2<Payload>>(config, "BoundedList2");

  // malloc_stats_print(nullptr, nullptr, nullptr);
  return 0;
}
