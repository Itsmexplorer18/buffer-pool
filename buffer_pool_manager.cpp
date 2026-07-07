
#pragma once
#include "common.h"
#include "lru_k_replacer.h"
#include "disk_scheduler.h"
 //page gaurds read page gaurd write page gaurd using RAII concepts in c++
class FrameHeader {
public:
    page_id_t page_id_ = INVALID_PAGE_ID;
    std::atomic<int> pin_count_ = 0;
    bool is_dirty_ = false;
    std::shared_mutex latch_;
    
private:
    char data_[PAGE_SIZE];
    
public:
    char* GetData() { return data_; }
    const char* GetData() const { return data_; }
    
    void ResetData() {
        page_id_ = INVALID_PAGE_ID;
        pin_count_ = 0;
        is_dirty_ = false;
        memset(data_, 0,PAGE_SIZE);
    }
};
class BufferPoolManager;
class ReadPageGuard;
class WritePageGuard;

class ReadPageGuard {
private:
    BufferPoolManager* bpm_;
    FrameHeader* frame_;
    
public:
    ReadPageGuard() : bpm_(nullptr), frame_(nullptr) {}
    
    ReadPageGuard(BufferPoolManager* bpm, FrameHeader* frame) 
        : bpm_(bpm), frame_(frame) {
         frame_->latch_.lock_shared();
        }
    
    ReadPageGuard(const ReadPageGuard&) = delete;
    ReadPageGuard& operator=(const ReadPageGuard&) = delete;
    
    ReadPageGuard(ReadPageGuard&& other) noexcept 
        : bpm_(other.bpm_), frame_(other.frame_) {
        other.bpm_ = nullptr;
        other.frame_ = nullptr;
    }
    
    ReadPageGuard& operator=(ReadPageGuard&& other) noexcept {
        if (this != &other) {
            Drop();
            bpm_ = other.bpm_;
            frame_ = other.frame_;
            other.bpm_ = nullptr;
            other.frame_ = nullptr;
        }
        return *this;
    }
    
    ~ReadPageGuard() { Drop(); }
    
    void Drop();
    
    const char* GetData() const {
        return frame_ ? frame_->GetData() : nullptr;
    }
    
    page_id_t GetPageId() const {
        return frame_ ? frame_->page_id_ : INVALID_PAGE_ID;
    }
};

class WritePageGuard {
private:
    BufferPoolManager* bpm_;
    FrameHeader* frame_;
    
public:
    WritePageGuard() : bpm_(nullptr), frame_(nullptr) {}
    
    WritePageGuard(BufferPoolManager* bpm, FrameHeader* frame) 
        : bpm_(bpm), frame_(frame) {
           frame_->latch_.lock();         // exclusive, blocks all readers and writers
        }
    
    WritePageGuard(const WritePageGuard&) = delete;
    WritePageGuard& operator=(const WritePageGuard&) = delete;
    
    WritePageGuard(WritePageGuard&& other) noexcept 
        : bpm_(other.bpm_), frame_(other.frame_) {
        other.bpm_ = nullptr;
        other.frame_ = nullptr;
    }
    
    WritePageGuard& operator=(WritePageGuard&& other) noexcept {
        if (this != &other) {
            Drop();
            bpm_ = other.bpm_;
            frame_ = other.frame_;
            other.bpm_ = nullptr;
            other.frame_ = nullptr;
        }
        return *this;
    }
    
    ~WritePageGuard() { Drop(); }
    
    void Drop();
    
    char* GetDataMut() {
        if (frame_) {
            frame_->is_dirty_ = true;
        }
        return frame_ ? frame_->GetData() : nullptr;
    }
    
    const char* GetData() const {
        return frame_ ? frame_->GetData() : nullptr;
    }
    
    page_id_t GetPageId() const {
        return frame_ ? frame_->page_id_ : INVALID_PAGE_ID;
    }
};

class BufferPoolManager {
private:
    size_t pool_size_;
    std::atomic<page_id_t> next_page_id_;
    std::unique_ptr<FrameHeader[]> frames_;
    std::unordered_map<page_id_t, frame_id_t> page_table_;
    std::queue<frame_id_t> free_list_;
    std::unique_ptr<LRUKReplacer> replacer_;
    std::unique_ptr<DiskScheduler> disk_scheduler_;
    mutable std::mutex latch_;
    std::optional<frame_id_t> FindFreeFrame() {
        if (!free_list_.empty()) {
            frame_id_t frame_id = free_list_.front();
            free_list_.pop();
            return frame_id;
        }
        return replacer_->Evict();
    }
    bool FlushFrame(frame_id_t frame_id) {
        FrameHeader& frame = frames_[frame_id]; 
        if (frame.page_id_ == INVALID_PAGE_ID) {
            return false;
        }
        
        if (frame.is_dirty_) {
            auto request = std::make_unique<DiskRequest>(true, frame.GetData(), frame.page_id_);
            auto future = request->callback.get_future();
            disk_scheduler_->Schedule(std::move(request));
             bool success = future.get();
            if (success) {
                frame.is_dirty_ = false;
            }
            return success;
        }
        
        return true;
    }

public:
    BufferPoolManager(size_t pool_size, std::unique_ptr<DiskManager> disk_manager, 
                     size_t replacer_k = 2)
        : pool_size_(pool_size), next_page_id_(0) {
        frames_ = std::make_unique<FrameHeader[]>(pool_size_);
        
        for (size_t i = 0; i < pool_size_; ++i) {
            free_list_.push(static_cast<frame_id_t>(i));
        }
                replacer_ = std::make_unique<LRUKReplacer>(pool_size_, replacer_k);
        disk_scheduler_ = std::make_unique<DiskScheduler>(std::move(disk_manager));
    }
    
    ~BufferPoolManager() = default;
    
    page_id_t NewPage() {
        std::lock_guard<std::mutex> lock(latch_);
        
        auto frame_id_opt = FindFreeFrame();
        if (!frame_id_opt) {
            return INVALID_PAGE_ID;
        }
        
        frame_id_t frame_id = *frame_id_opt;
        FrameHeader& frame = frames_[frame_id];
        
        if (frame.page_id_ != INVALID_PAGE_ID) {
            page_table_.erase(frame.page_id_);
            FlushFrame(frame_id);
        }       
        page_id_t new_page_id = next_page_id_++;
        frame.ResetData();
        frame.page_id_ = new_page_id;
        frame.pin_count_ = 1;
                page_table_[new_page_id] = frame_id;
        replacer_->RecordAccess(frame_id);
        replacer_->SetEvictable(frame_id, false);        
        return new_page_id;
    }
    
    bool DeletePage(page_id_t page_id) {
    std::lock_guard<std::mutex> lock(latch_);

    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return true;
    }

    frame_id_t frame_id = it->second;
    FrameHeader& frame = frames_[frame_id];

    if (frame.pin_count_ > 0) {
        return false;
    }

    // NOTE: no WAL in this implementation so flush dirty data
    // before discarding to prevent data loss on deletion.
    // If extended with a transaction manager and WAL, remove
    // this flush — WAL log record handles crash recovery instead.
     //right not delete oage is an public api not being called by any internal code might be used to extend code for another levels 
    if (frame.is_dirty_) {
        FlushFrame(frame_id);
    }

    page_table_.erase(page_id);
    replacer_->Remove(frame_id);
    frame.ResetData();
    free_list_.push(frame_id);

    return true;
}
    
    std::optional<ReadPageGuard> CheckedReadPage(page_id_t page_id) {
     {
        std::lock_guard<std::mutex> lock(latch_);
        
        frame_id_t frame_id;
         auto it = page_table_.find(page_id);
        if (it != page_table_.end()) {
            frame_id = it->second;
        } else {
            auto frame_id_opt = FindFreeFrame();
            if (!frame_id_opt) {
                return std::nullopt;
            } 
            frame_id = *frame_id_opt;
            FrameHeader& frame = frames_[frame_id];
            
            if (frame.page_id_ != INVALID_PAGE_ID) {
                page_table_.erase(frame.page_id_);
                FlushFrame(frame_id);
            }
            
            // Load page from disk
            frame.ResetData();
            frame.page_id_ = page_id;
            
            auto request = std::make_unique<DiskRequest>(false, frame.GetData(), page_id);
            auto future = request->callback.get_future();
            disk_scheduler_->Schedule(std::move(request));
            
            bool success = future.get();
            if (!success) {
                frame.ResetData();
                free_list_.push(frame_id);
                return std::nullopt;
            }
            
            page_table_[page_id] = frame_id;
        }
        
        FrameHeader& frame = frames_[frame_id];
        frame.pin_count_++;
        replacer_->RecordAccess(frame_id);
        replacer_->SetEvictable(frame_id, false);
     }     
        return ReadPageGuard(this, &frame);
    }
    
    std::optional<WritePageGuard> CheckedWritePage(page_id_t page_id) {
        std::lock_guard<std::mutex> lock(latch_);
        
        frame_id_t frame_id;
        
        auto it = page_table_.find(page_id);
        if (it != page_table_.end()) {
            frame_id = it->second;
        } else {
            // Need to load page from disk
            auto frame_id_opt = FindFreeFrame();
            if (!frame_id_opt) {
                return std::nullopt;
            }
            
            frame_id = *frame_id_opt;
            FrameHeader& frame = frames_[frame_id];
            
            if (frame.page_id_ != INVALID_PAGE_ID) {
                page_table_.erase(frame.page_id_);
                FlushFrame(frame_id);
            }
            
            frame.ResetData();
            frame.page_id_ = page_id;
            
            auto request = std::make_unique<DiskRequest>(false, frame.GetData(), page_id);
            auto future = request->callback.get_future();
            disk_scheduler_->Schedule(std::move(request));
            
            // Wait for completion
            bool success = future.get();
            if (!success) {
                frame.ResetData();
                free_list_.push(frame_id);
                return std::nullopt;
            }
            
            page_table_[page_id] = frame_id;
        }
        
        FrameHeader& frame = frames_[frame_id];
     
       if (frame.pin_count_ > 0) {
        return std::nullopt;
    }
        frame.pin_count_++;
        replacer_->RecordAccess(frame_id);
        replacer_->SetEvictable(frame_id, false);
        
        return WritePageGuard(this, &frame);
    }
    
    bool FlushPage(page_id_t page_id) {
        std::lock_guard<std::mutex> lock(latch_);
        
        auto it = page_table_.find(page_id);
        if (it == page_table_.end()) {
            return false;
        }
        
        return FlushFrame(it->second);
    }
    
    void FlushAllPages() {
        std::lock_guard<std::mutex> lock(latch_);
        
        for (const auto& [page_id, frame_id] : page_table_) {
            FlushFrame(frame_id);
        }
    }
    
    int GetPinCount(page_id_t page_id) {
        std::lock_guard<std::mutex> lock(latch_);
        
        auto it = page_table_.find(page_id);
        if (it == page_table_.end()) {
            return 0;
        }
        
        return frames_[it->second].pin_count_.load();
    }
    
    void UnpinPage(page_id_t page_id, bool is_dirty) {
        std::lock_guard<std::mutex> lock(latch_);
        
        auto it = page_table_.find(page_id);
        if (it == page_table_.end()) {
            return;
        }
        
        frame_id_t frame_id = it->second;
        FrameHeader& frame = frames_[frame_id];
        
        if (frame.pin_count_ > 0) {
            frame.pin_count_--;
            if (is_dirty) {
                frame.is_dirty_ = true;
            }
            
            
            if (frame.pin_count_ == 0) {
                replacer_->SetEvictable(frame_id, true);
            }
        }
    }
};

void ReadPageGuard::Drop() {
    if (bpm_ && frame_) {
                 frame_->latch_.unlock_shared();
        bpm_->UnpinPage(frame_->page_id_, false);
        bpm_ = nullptr;
        frame_ = nullptr;
    }
}
void WritePageGuard::Drop() {
    if (bpm_ && frame_) {
          frame_->latch_.unlock();
        bpm_->UnpinPage(frame_->page_id_, true);
        bpm_ = nullptr;
        frame_ = nullptr;
    }
}
