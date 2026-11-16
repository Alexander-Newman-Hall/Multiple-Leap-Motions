#pragma once
#include <memory>
#include <mutex>
#include <utility>

namespace handbridge {
// One immutable published value, with reader-owned lifetime across replacements.
// Construct/serialize snapshots before store; only pointer exchange holds the lock.
// This deliberately avoids the C++20 atomic<shared_ptr> specialization, which is
// absent from the measured Apple libc++ headers. C++20 remains required.
template<class T> class SnapshotMailbox {
public:
    std::shared_ptr<const T> load() const {
        std::lock_guard lock(mutex_);
        return value_;
    }
    void store(std::shared_ptr<const T> next) {
        {
            std::lock_guard lock(mutex_);
            value_.swap(next);
        }
        // Retire the previous value outside the lock; readers may still own it.
    }
private:
    mutable std::mutex mutex_;
    std::shared_ptr<const T> value_;
};
}
