
#pragma once
#include "common.h"

class DiskManager {
public:
    void ReadPage(page_id_t page_id, char* page_data) {
        // Simulate disk read
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        memset(page_data, static_cast<int>(page_id), BUSTUB_PAGE_SIZE);
    }
    
    void WritePage(page_id_t page_id, const char* page_data) {
        // Simulate disk write
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // In real implementation, would write to disk
    }
};

struct DiskRequest {
    bool is_write;
    char* data;
    page_id_t page_id;
    std::promise<bool> callback;
    
    DiskRequest(bool is_write, char* data, page_id_t page_id)
        : is_write(is_write), data(data), page_id(page_id) {}
};

// Thread-safe channel for requests
template<typename T>
class Channel {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool closed_ = false;

public:
    void Put(T item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!closed_) {
            queue_.push(std::move(item));
            cv_.notify_one();
        }
    }
    
    std::optional<T> Get() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || closed_; });
        
        if (queue_.empty()) {
            return std::nullopt;
        }
        
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }
    
    void Close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        cv_.notify_all();
    }
};

class DiskScheduler {
private:
    std::unique_ptr<DiskManager> disk_manager_;
    Channel<std::unique_ptr<DiskRequest>> request_queue_;
    std::thread worker_thread_;
    
public:
    explicit DiskScheduler(std::unique_ptr<DiskManager> disk_manager)
        : disk_manager_(std::move(disk_manager)) {
        worker_thread_ = std::thread([this] { StartWorkerThread(); });
    }
    
    ~DiskScheduler() {
        request_queue_.Close();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    void Schedule(std::unique_ptr<DiskRequest> request) {
        request_queue_.Put(std::move(request));
    }
    
private:
    void StartWorkerThread() {
        while (auto request_opt = request_queue_.Get()) {
            auto request = std::move(*request_opt);
            
            try {
                if (request->is_write) {
                    disk_manager_->WritePage(request->page_id, request->data);
                } else {
                    disk_manager_->ReadPage(request->page_id, request->data);
                }
                request->callback.set_value(true);
            } catch (...) {
                request->callback.set_value(false);
            }
        }
    }
};
