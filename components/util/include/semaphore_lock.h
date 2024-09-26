#ifndef SEMAPHORE_LOCK_H
#define SEMAPHORE_LOCK_H

#include <semaphore>

/// @brief A helper class to automatically acquire and release a semaphore within a scope
template<uint8_t T>
class SemaphoreLock {
public:
    /// @brief Acquires a semaphore and stores it for release on destruction
    /// @param semaphore The semaphore to acquire
    inline explicit SemaphoreLock(std::counting_semaphore<T> &semaphore) noexcept {
        this->semaphore = &semaphore;
        this->semaphore->acquire();
    }

    inline void release() noexcept {
        if (this->is_released) return;
        this->semaphore->release();
        this->is_released = true;
    }

    /// @brief Releases the semaphore
    inline ~SemaphoreLock() noexcept {
        this->release();
    }

private:
    /// @brief The semaphore to control
    std::counting_semaphore<T>* semaphore;

    /// @brief Indicates whether this semaphore has already been released
    bool is_released = false;
};

#endif //SEMAPHORE_LOCK_H
