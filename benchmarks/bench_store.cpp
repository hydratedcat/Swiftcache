#include <benchmark/benchmark.h>

#include "../src/core/lru.h"

#include <string>

// ---------------------------------------------------------------------------
// Named constants
// ---------------------------------------------------------------------------
namespace {
constexpr size_t CACHE_CAPACITY = 1000000;
constexpr size_t PREFILL_COUNT = 1000000;
} // namespace

// ---------------------------------------------------------------------------
// BM_Set: single-threaded PUT throughput
// ---------------------------------------------------------------------------
// Measures raw insert speed into an LRU cache with 1M capacity.
// Each iteration inserts a unique key to avoid the update-existing path.
static void BM_Set(benchmark::State &state) {
  LRUCache cache(CACHE_CAPACITY);
  int i = 0;
  for (auto _ : state) {
    cache.put(std::to_string(i++), "value");
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Set);

// ---------------------------------------------------------------------------
// BM_Get: single-threaded GET throughput (cache hits)
// ---------------------------------------------------------------------------
// Pre-fills the cache with 1M entries, then measures read speed.
// Keys cycle via modulo to guarantee 100% hit rate.
static void BM_Get(benchmark::State &state) {
  LRUCache cache(CACHE_CAPACITY);
  for (size_t i = 0; i < PREFILL_COUNT; ++i) {
    cache.put(std::to_string(i), "value");
  }
  int i = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        cache.get(std::to_string(i++ % static_cast<int>(PREFILL_COUNT))));
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Get);

// ---------------------------------------------------------------------------
// BM_Del: single-threaded DELETE throughput
// ---------------------------------------------------------------------------
// Pre-fills cache, then deletes keys sequentially.
// Measures teardown path (erase from list + map).
static void BM_Del(benchmark::State &state) {
  LRUCache cache(CACHE_CAPACITY);
  for (size_t i = 0; i < PREFILL_COUNT; ++i) {
    cache.put(std::to_string(i), "value");
  }
  int i = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        cache.del(std::to_string(i++ % static_cast<int>(PREFILL_COUNT))));
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Del);

// ---------------------------------------------------------------------------
// BM_SetGet_Mixed: single-threaded 50/50 SET+GET workload
// ---------------------------------------------------------------------------
// Alternates SET and GET on the same key space. Represents a realistic
// read-write mix where ~50% of operations are writes.
static void BM_SetGet_Mixed(benchmark::State &state) {
  LRUCache cache(CACHE_CAPACITY);
  // Pre-fill half the capacity so GETs have data to hit
  for (size_t i = 0; i < PREFILL_COUNT / 2; ++i) {
    cache.put(std::to_string(i), "value");
  }
  int i = 0;
  for (auto _ : state) {
    if (i % 2 == 0) {
      cache.put(std::to_string(i), "value");
    } else {
      benchmark::DoNotOptimize(
          cache.get(std::to_string(i % static_cast<int>(PREFILL_COUNT))));
    }
    ++i;
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SetGet_Mixed);

// ---------------------------------------------------------------------------
// BM_Set_Eviction: single-threaded SET with continuous eviction
// ---------------------------------------------------------------------------
// Uses a small cache (1000 entries) and inserts unique keys endlessly.
// After the first 1000, every insert triggers an eviction — measures the
// combined cost of evict_lru() + emplace_front + map insert.
static void BM_Set_Eviction(benchmark::State &state) {
  constexpr size_t SMALL_CAPACITY = 1000;
  LRUCache cache(SMALL_CAPACITY);
  int i = 0;
  for (auto _ : state) {
    cache.put(std::to_string(i++), "value");
  }
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Set_Eviction);

// ---------------------------------------------------------------------------
// Multi-threaded benchmarks: same cache shared across N threads
// ---------------------------------------------------------------------------
// These use a static cache so all threads operate on the same instance,
// exercising the mutex contention path under load.

// BM_Set_MultiThread: concurrent PUT
static void BM_Set_MultiThread(benchmark::State &state) {
  static LRUCache *cache = nullptr;
  if (state.thread_index() == 0) {
    cache = new LRUCache(CACHE_CAPACITY);
  }
  int i = state.thread_index() * 10000000;
  for (auto _ : state) {
    cache->put(std::to_string(i++), "value");
  }
  state.SetItemsProcessed(state.iterations());
  if (state.thread_index() == 0) {
    delete cache;
    cache = nullptr;
  }
}
BENCHMARK(BM_Set_MultiThread)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

// BM_Get_MultiThread: concurrent GET (100% hit rate)
static void BM_Get_MultiThread(benchmark::State &state) {
  static LRUCache *cache = nullptr;
  if (state.thread_index() == 0) {
    cache = new LRUCache(CACHE_CAPACITY);
    for (size_t i = 0; i < PREFILL_COUNT; ++i) {
      cache->put(std::to_string(i), "value");
    }
  }
  int i = state.thread_index() * 10000000;
  for (auto _ : state) {
    benchmark::DoNotOptimize(
        cache->get(std::to_string(i++ % static_cast<int>(PREFILL_COUNT))));
  }
  state.SetItemsProcessed(state.iterations());
  if (state.thread_index() == 0) {
    delete cache;
    cache = nullptr;
  }
}
BENCHMARK(BM_Get_MultiThread)->Threads(1)->Threads(2)->Threads(4)->Threads(8);

BENCHMARK_MAIN();
