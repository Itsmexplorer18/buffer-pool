
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
    std::mutex latch_;
    
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
        : bpm_(bpm), frame_(frame) {}
    
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
        : bpm_(bpm), frame_(frame) {}
    
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

