
#pragma once

#include "common.h"
class LRUKReplacer {
private:
    size_t num_frames_;
    size_t k_;
    std::atomic<timestamp_t> current_timestamp_;
        std::unordered_map<frame_id_t, std::deque<timestamp_t>> access_history_;
        std::unordered_set<frame_id_t> evictable_frames_;
        mutable std::mutex latch_;
    
    size_t GetBackwardKDistance(frame_id_t frame_id) const {
        auto it = access_history_.find(frame_id);
        if (it == access_history_.end() || it->second.size() < k_) {
            return SIZE_MAX; // +infinity
        }
        
        // Return difference between current time and kth previous access
        timestamp_t kth_access = it->second.front();
        return current_timestamp_.load() - kth_access;
    }

public:
    explicit LRUKReplacer(size_t num_frames, size_t k) 
        : num_frames_(num_frames), k_(k), current_timestamp_(0) {}
    
    ~LRUKReplacer() = default;
    
    std::optional<frame_id_t> Evict() {
        std::lock_guard<std::mutex> lock(latch_);
        
        if (evictable_frames_.empty()) {
            return std::nullopt;
        }
        
        frame_id_t victim_frame = INVALID_FRAME_ID;
        size_t max_backward_distance = 0;
        timestamp_t earliest_access = current_timestamp_.load();
        
        // Find frame with maximum backward k-distance
        for (frame_id_t frame_id : evictable_frames_) {
            size_t distance = GetBackwardKDistance(frame_id);
            
            if (distance == SIZE_MAX) {
                auto it = access_history_.find(frame_id);
                if (it != access_history_.end() && !it->second.empty()) {
                    timestamp_t earliest = it->second.front();
                    if (victim_frame == INVALID_FRAME_ID || earliest < earliest_access) {
                        victim_frame = frame_id;
                        earliest_access = earliest;
                    }
                }
            } else if (victim_frame == INVALID_FRAME_ID || distance > max_backward_distance) {
                victim_frame = frame_id;
                max_backward_distance = distance;
            }
        }
        
        if (victim_frame != INVALID_FRAME_ID) {
            evictable_frames_.erase(victim_frame);
            access_history_.erase(victim_frame);
        }
        
        return victim_frame;
    }
    
    void RecordAccess(frame_id_t frame_id) {
        std::lock_guard<std::mutex> lock(latch_);
        
        timestamp_t current_time = ++current_timestamp_;
        
        auto& history = access_history_[frame_id];
        history.push_back(current_time);
        
        // Keep only k timestamps
        if (history.size() > k_) {
            history.pop_front();
        }
    }
    
    void Remove(frame_id_t frame_id) {
        std::lock_guard<std::mutex> lock(latch_);
        
        access_history_.erase(frame_id);
        evictable_frames_.erase(frame_id);
    }
    
    void SetEvictable(frame_id_t frame_id, bool set_evictable) {
        std::lock_guard<std::mutex> lock(latch_);
        
        if (set_evictable) {
            evictable_frames_.insert(frame_id);
        } else {
            evictable_frames_.erase(frame_id);
        }
    }
    
    size_t Size() const {
        std::lock_guard<std::mutex> lock(latch_);
        return evictable_frames_.size();
    }
};
