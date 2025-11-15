// =============================================================================
// COMMON TYPES AND INCLUDES
// =============================================================================
#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <future>
#include <optional>
#include <memory>
#include <fstream>
#include <cassert>
#include <cstring>
#include <deque>

using frame_id_t = int32_t;
using page_id_t = int32_t;
using timestamp_t = size_t;

constexpr size_t BUSTUB_PAGE_SIZE = 4096;
constexpr frame_id_t INVALID_FRAME_ID = -1;
constexpr page_id_t INVALID_PAGE_ID = -1;
