#include "ResourceManager.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Function to run a reader thread
void reader_thread(std::shared_ptr<ResourceManager<std::string>> manager,
                   int id, int num_reads, std::atomic<int> &completed_reads) {

  std::cout << "Reader " << id << " starting..." << std::endl;

  for (int i = 0; i < num_reads; ++i) {
    auto result = manager->read([](const std::string &resource) {
      // Just read the resource
      std::this_thread::sleep_for(
          std::chrono::milliseconds(1)); // Simulate some work
      return resource.length();
    });

    completed_reads.fetch_add(1, std::memory_order_relaxed);

    if (i % 100 == 0) {
      std::cout << "Reader " << id << " completed " << i
                << " reads, current value length: " << result << std::endl;
    }
  }

  std::cout << "Reader " << id << " finished" << std::endl;
}

// Function to run a writer thread
void writer_thread(std::shared_ptr<ResourceManager<std::string>> manager,
                   int num_updates, std::atomic<int> &completed_updates) {

  std::cout << "Writer starting..." << std::endl;

  for (int i = 0; i < num_updates; ++i) {
    auto new_value =
        std::make_unique<std::string>("Updated resource " + std::to_string(i));
    auto [old_value, epoch] = manager->update(std::move(new_value));
    while (!manager->can_reclaim(epoch)) {
      std::this_thread::yield();
    }

    completed_updates.fetch_add(1, std::memory_order_relaxed);
    std::cout << "Updated to: " << "Updated resource " << i << std::endl;

    // Wait between updates
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  std::cout << "Writer finished" << std::endl;
}

int main() {
  std::cout << "Testing ResourceManager with strings" << std::endl;

  // Create a ResourceManager with an initial string
  auto manager = std::make_shared<ResourceManager<std::string>>(
      std::make_unique<std::string>("Initial resource"));

  // Test parameters
  const int num_reader_threads = 4;
  const int reads_per_thread = 500;
  const int num_updates = 20;

  // Counters for completed operations
  std::atomic<int> completed_reads(0);
  std::atomic<int> completed_updates(0);

  // Start with a simple read test
  auto initial_length = manager->read(
      [](const std::string &resource) { return resource.length(); });

  std::cout << "Initial resource length: " << initial_length << std::endl;

  // Create and start reader threads
  std::vector<std::thread> readers;
  for (int i = 0; i < num_reader_threads; ++i) {
    readers.emplace_back(reader_thread, manager, i, reads_per_thread,
                         std::ref(completed_reads));
  }

  // Give readers a head start
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Create and start writer thread
  std::thread writer(writer_thread, manager, num_updates,
                     std::ref(completed_updates));

  // Periodically show progress and try cleanup
  auto start_time = std::chrono::steady_clock::now();
  while (completed_reads.load() < num_reader_threads * reads_per_thread ||
         completed_updates.load() < num_updates) {

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - start_time)
                       .count();

    std::cout << "Status after " << elapsed << "s: " << completed_reads.load()
              << "/" << (num_reader_threads * reads_per_thread) << " reads, "
              << completed_updates.load() << "/" << num_updates << " updates"
              << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Avoid infinite loop if something goes wrong
    if (elapsed > 60) {
      std::cout << "Test taking too long, stopping..." << std::endl;
      break;
    }
  }

  // Wait for all threads to finish
  writer.join();
  for (auto &thread : readers) {
    thread.join();
  }

  // Final check
  auto final_length = manager->read(
      [](const std::string &resource) { return resource.length(); });

  auto final_value =
      manager->read([](const std::string &resource) { return resource; });

  std::cout << "\nTest completed!" << std::endl;
  std::cout << "Final resource: " << final_value << std::endl;
  std::cout << "Final resource length: " << final_length << std::endl;
  std::cout << "Total reads: " << completed_reads.load() << std::endl;
  std::cout << "Total updates: " << completed_updates.load() << std::endl;

  return 0;
}
