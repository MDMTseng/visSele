#pragma once

/**
 * @brief Lock abstraction interface
 * 
 * This interface provides a platform-agnostic way to handle mutex/lock
 * operations without direct dependencies on specific threading implementations.
 */
class ILock {
public:
    virtual ~ILock() = default;
    
    /**
     * @brief Acquire the lock
     * @param timeout_ms Maximum time to wait in milliseconds (0 = no wait, -1 = wait forever)
     * @return true if lock acquired, false on timeout
     */
    virtual bool lock(int32_t timeout_ms = -1) = 0;
    
    /**
     * @brief Release the lock
     * @return true if successful, false otherwise
     */
    virtual bool unlock() = 0;
    
    /**
     * @brief Try to acquire the lock without waiting
     * @return true if lock acquired, false if already locked
     */
    virtual bool tryLock() = 0;
};

/**
 * @brief RAII-style lock guard
 * 
 * Automatically acquires lock on construction and releases on destruction
 */
class LockGuard {
private:
    ILock& lock_;
    bool locked_;
    
public:
    /**
     * @brief Construct a new Lock Guard object
     * @param lock Lock to acquire
     * @param timeout_ms Maximum time to wait in milliseconds
     */
    explicit LockGuard(ILock& lock, int32_t timeout_ms = -1) : lock_(lock), locked_(false) {
        locked_ = lock_.lock(timeout_ms);
    }
    
    /**
     * @brief Destroy the Lock Guard object and release the lock if acquired
     */
    ~LockGuard() {
        if (locked_) {
            lock_.unlock();
        }
    }
    
    /**
     * @brief Check if lock was successfully acquired
     * @return true if lock is held, false otherwise
     */
    bool isLocked() const {
        return locked_;
    }
    
    // Prevent copying
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
};