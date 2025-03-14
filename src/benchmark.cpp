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
#include <string>
#include <thread>
#include <vector>

// Command line argument parser
struct BenchmarkConfig {
  int reader_threads = 4;
  int duration_seconds = 10;
  int updates_per_second = 100;
  bool csv_output = false;
  std::string output_file = "benchmark_results.csv";

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

public:
  explicit ReaderStats(int id) : thread_id(id) {
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
    std::cout << "Sorting latencies..." << std::endl;
    std::sort(latencies.begin(), latencies.end());
    std::cout << "Done." << std::endl;
  }

  void print_stats() const {
    std::cout << "Thread " << thread_id << ":" << std::endl;
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
    return "thread_id,total_reads,reads_per_sec,median_latency_us,avg_latency_"
           "us,"
           "p90_latency_us,p99_latency_us,p999_latency_us";
  }

  std::string get_csv_row() const {
    std::stringstream ss;
    ss << thread_id << "," << total_reads << "," << std::fixed
       << std::setprecision(2) << get_reads_per_second() << "," << std::fixed
       << std::setprecision(2) << get_percentile(0.5) << "," << std::fixed
       << std::setprecision(2) << get_average_latency() << "," << std::fixed
       << std::setprecision(2) << get_percentile(0.9) << "," << std::fixed
       << std::setprecision(2) << get_percentile(0.99) << "," << std::fixed
       << std::setprecision(2) << get_percentile(0.999);
    return ss.str();
  }
};

// Reader thread function
void reader_function(std::shared_ptr<ResourceManager<std::string>> manager,
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

// Writer thread function
void writer_function(std::shared_ptr<ResourceManager<std::string>> manager,
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

int main(int argc, char *argv[]) {
  // Parse command line arguments
  BenchmarkConfig config = BenchmarkConfig::parse_args(argc, argv);

  std::cout << "Starting benchmark with:" << std::endl;
  std::cout << "  Reader threads: " << config.reader_threads << std::endl;
  std::cout << "  Duration: " << config.duration_seconds << " seconds"
            << std::endl;
  std::cout << "  Writer updates: " << config.updates_per_second
            << " per second" << std::endl;

  // Create a resource manager with initial string
  auto manager = std::make_shared<ResourceManager<std::string>>(
      std::make_unique<std::string>("Initial resource"));

  // Create a stop flag to signal threads to stop
  std::atomic<bool> should_stop(false);

  // Create reader statistics objects
  std::vector<std::unique_ptr<ReaderStats>> all_stats;
  for (int i = 0; i < config.reader_threads; ++i) {
    all_stats.emplace_back(std::make_unique<ReaderStats>(i));
  }

  // Create and start reader threads
  std::vector<std::thread> reader_threads;
  for (int i = 0; i < config.reader_threads; ++i) {
    reader_threads.emplace_back(reader_function, manager,
                                std::ref(*all_stats[i]), std::ref(should_stop));
  }

  // Create atomic counter for updates
  std::atomic<uint64_t> update_counter(0);

  // Create and start writer thread
  std::thread writer_thread(writer_function, manager, config.updates_per_second,
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
  std::cout << "\nBenchmark Results:" << std::endl;
  std::cout << "=================" << std::endl;

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

  std::cout << "Aggregate Statistics:" << std::endl;
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

  // Output CSV if requested
  if (config.csv_output) {
    std::ofstream csv_file(config.output_file);

    if (csv_file.is_open()) {
      // Write header
      csv_file << all_stats[0]->get_csv_header() << std::endl;

      // Write data rows
      for (const auto &stats : all_stats) {
        csv_file << stats->get_csv_row() << std::endl;
      }

      // Write aggregate row
      csv_file << "aggregate," << total_reads << "," << std::fixed
               << std::setprecision(2) << reads_per_second << ",,,,,";

      std::cout << "CSV results written to " << config.output_file << std::endl;
    } else {
      std::cerr << "Error: Could not open file for CSV output." << std::endl;
    }
  }

  return 0;
}
