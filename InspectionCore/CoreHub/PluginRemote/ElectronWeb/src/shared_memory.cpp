#include <napi.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <functional>
#include <queue>
#include <condition_variable>

// Platform detection
#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__) || defined(__CYGWIN__)
#define IS_WINDOWS 1
#include <windows.h>
#else
#define IS_WINDOWS 0
#endif

// Boost.Interprocess includes
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/sync/named_semaphore.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// Constants
const size_t BUFFER_SIZE = 4096;
const std::string CHANNEL_NAME = "TestChannel";

// Shared memory data structure (must match the one in PluginTemp.cpp)
struct SharedMemoryData {
    SharedMemoryData() : sender_ready(1), receiver_ready(0), complete_signal(0), data_size(0) {}
    
    boost::interprocess::interprocess_semaphore sender_ready;
    boost::interprocess::interprocess_semaphore receiver_ready;
    boost::interprocess::interprocess_semaphore complete_signal;
    
    size_t data_size;
    // Data buffer follows this structure in memory
};

// Thread-safe message queue for passing messages from the receiver thread to the main thread
class MessageQueue {
private:
    std::queue<std::string> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    
public:
    void push(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(message);
        cond_.notify_one();
    }
    
    bool pop(std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        
        message = queue_.front();
        queue_.pop();
        return true;
    }
    
    bool empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    void wait_and_pop(std::string& message) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !queue_.empty(); });
        message = queue_.front();
        queue_.pop();
    }
};

// Simplified shared memory channel class (similar to the one in PluginTemp.cpp)
class smem_channel {
private:
    std::string m_name;
    size_t m_memSize;
    bool m_isCreator;
    bool m_useNamedSemaphores;
    std::atomic<bool> m_shouldExit;
    
    boost::interprocess::shared_memory_object* m_shm;
    boost::interprocess::mapped_region* m_region;
    SharedMemoryData* m_data;
    char* m_buffer;
    
    // Named semaphores for Windows
    boost::interprocess::named_semaphore* m_senderReadySem;
    boost::interprocess::named_semaphore* m_receiverReadySem;
    boost::interprocess::named_semaphore* m_completeSem;
    
public:
    // Constructor with option to create the shared memory
    smem_channel(const std::string name, size_t memSize, bool isCreator) 
        : m_name(name),
          m_memSize(memSize),
          m_isCreator(isCreator),
          m_useNamedSemaphores(IS_WINDOWS),
          m_shouldExit(false),
          m_shm(nullptr), 
          m_region(nullptr), 
          m_data(nullptr),
          m_buffer(nullptr),
          m_senderReadySem(nullptr),
          m_receiverReadySem(nullptr),
          m_completeSem(nullptr) {
        
        try {
            if (isCreator) {
                // First, remove any existing shared memory with the same name
                boost::interprocess::shared_memory_object::remove(m_name.c_str());
                
                if (m_useNamedSemaphores) {
                    // Remove any existing named semaphores
                    boost::interprocess::named_semaphore::remove((m_name + "_sender").c_str());
                    boost::interprocess::named_semaphore::remove((m_name + "_receiver").c_str());
                    boost::interprocess::named_semaphore::remove((m_name + "_complete").c_str());
                    
                    // Create named semaphores
                    m_senderReadySem = new boost::interprocess::named_semaphore(
                        boost::interprocess::create_only,
                        (m_name + "_sender").c_str(),
                        1  // Initial value - sender starts ready
                    );
                    
                    m_receiverReadySem = new boost::interprocess::named_semaphore(
                        boost::interprocess::create_only,
                        (m_name + "_receiver").c_str(),
                        0  // Initial value - receiver starts not ready
                    );
                    
                    m_completeSem = new boost::interprocess::named_semaphore(
                        boost::interprocess::create_only,
                        (m_name + "_complete").c_str(),
                        0  // Initial value - complete signal starts not ready
                    );
                }
                
                // Create shared memory
                m_shm = new boost::interprocess::shared_memory_object(
                    boost::interprocess::create_only,
                    m_name.c_str(),
                    boost::interprocess::read_write
                );
                
                // Set size (structure + data buffer)
                m_shm->truncate(sizeof(SharedMemoryData) + memSize);
            } else {
                // Try to open existing shared memory
                m_shm = new boost::interprocess::shared_memory_object(
                    boost::interprocess::open_only,
                    m_name.c_str(),
                    boost::interprocess::read_write
                );
                
                if (m_useNamedSemaphores) {
                    // Open existing named semaphores
                    m_senderReadySem = new boost::interprocess::named_semaphore(
                        boost::interprocess::open_only,
                        (m_name + "_sender").c_str()
                    );
                    
                    m_receiverReadySem = new boost::interprocess::named_semaphore(
                        boost::interprocess::open_only,
                        (m_name + "_receiver").c_str()
                    );
                    
                    m_completeSem = new boost::interprocess::named_semaphore(
                        boost::interprocess::open_only,
                        (m_name + "_complete").c_str()
                    );
                }
            }
            
            // Map the whole shared memory in this process
            m_region = new boost::interprocess::mapped_region(
                *m_shm,
                boost::interprocess::read_write
            );
            
            // Get the address of the mapped region
            void* addr = m_region->get_address();
            
            if (isCreator) {
                // Construct the shared structure in the mapped region
                m_data = new (addr) SharedMemoryData;
            } else {
                // Get the shared structure from the mapped region
                m_data = static_cast<SharedMemoryData*>(addr);
            }
            
            // Set buffer pointer to the memory after the SharedMemoryData structure
            m_buffer = reinterpret_cast<char*>(addr) + sizeof(SharedMemoryData);
            
        } catch (const std::exception& e) {
            std::cerr << "smem_channel initialization error: " << e.what() << std::endl;
            cleanup();
            throw;
        }
    }
    
    ~smem_channel() {
        cleanup();
    }
    
    void cleanup() {
        if (m_region) {
            delete m_region;
            m_region = nullptr;
        }
        
        if (m_shm) {
            if (m_isCreator) {
                boost::interprocess::shared_memory_object::remove(m_name.c_str());
            }
            delete m_shm;
            m_shm = nullptr;
        }
        
        if (m_useNamedSemaphores) {
            if (m_senderReadySem) {
                delete m_senderReadySem;
                m_senderReadySem = nullptr;
                if (m_isCreator) {
                    boost::interprocess::named_semaphore::remove((m_name + "_sender").c_str());
                }
            }
            
            if (m_receiverReadySem) {
                delete m_receiverReadySem;
                m_receiverReadySem = nullptr;
                if (m_isCreator) {
                    boost::interprocess::named_semaphore::remove((m_name + "_receiver").c_str());
                }
            }
            
            if (m_completeSem) {
                delete m_completeSem;
                m_completeSem = nullptr;
                if (m_isCreator) {
                    boost::interprocess::named_semaphore::remove((m_name + "_complete").c_str());
                }
            }
        }
    }
    
    // Get the size of the shared memory buffer
    size_t size() {
        return m_memSize;
    }
    
    // Get a pointer to the shared memory buffer
    void* getPtr() {
        return m_buffer;
    }
    
    // Receiver waits for data
    bool r_wait(int timeout_ms = 100) {
        try {
            // Check if we should exit before waiting
            if (m_shouldExit) {
                return false;
            }
            
            bool success = false;
            
            if (m_useNamedSemaphores) {
                // For Windows, use named semaphores with timeout
                success = m_receiverReadySem->timed_wait(
                    boost::posix_time::microsec_clock::universal_time() + 
                    boost::posix_time::milliseconds(timeout_ms)
                );
            } else {
                // For non-Windows, use interprocess semaphores with try_wait
                success = m_data->receiver_ready.try_wait();
            }
            
            if (success) {
                // Ensure memory is synchronized after waking up
                if (m_region) {
                    m_region->flush();
                }
            }
            
            return success;
        } catch (const std::exception& e) {
            std::cerr << "r_wait error: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Receiver signals completion
    void r_release() {
        try {
            // Ensure memory writes are visible before signaling completion
            if (m_region) {
                m_region->flush();
            }
            
            if (m_useNamedSemaphores) {
                m_completeSem->post();
            } else {
                m_data->complete_signal.post();
            }
        } catch (const std::exception& e) {
            std::cerr << "r_release error: " << e.what() << std::endl;
        }
    }
    
    // Helper method to get data size
    size_t getDataSize() const {
        return m_data->data_size;
    }
    
    // Signal that the channel should exit
    void signalExit() {
        m_shouldExit = true;
    }
    
    bool shouldExit() const {
        return m_shouldExit;
    }
};

// Global variables
std::atomic<bool> g_running(false);
std::thread g_receiverThread;
MessageQueue g_messageQueue;
std::function<void(const std::string&)> g_messageCallback;
std::mutex g_callbackMutex;

// Function to check if shared memory exists
bool checkSharedMemoryExists(const std::string& name) {
    try {
        boost::interprocess::shared_memory_object shm(
            boost::interprocess::open_only,
            name.c_str(),
            boost::interprocess::read_only
        );
        return true;
    } catch (...) {
        return false;
    }
}

// Receiver thread function
void receiverThreadFunc() {
    std::cout << "Receiver thread started" << std::endl;
    
    smem_channel* channel = nullptr;
    
    try {
        // Try to open the channel
        int retries = 0;
        const int maxRetries = 10;
        
        while (retries < maxRetries && g_running) {
            try {
                std::cout << "Attempting to open shared memory channel (attempt " << retries+1 << ")" << std::endl;
                
                // Check if shared memory exists before trying to open it
                if (!checkSharedMemoryExists(CHANNEL_NAME)) {
                    std::cout << "Shared memory does not exist yet, waiting..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    retries++;
                    continue;
                }
                
                channel = new smem_channel(CHANNEL_NAME, BUFFER_SIZE, false);
                std::cout << "Successfully opened shared memory channel" << std::endl;
                break;  // Success
            } catch (const std::exception& e) {
                std::cerr << "Failed to open channel: " << e.what() << std::endl;
                retries++;
                if (retries >= maxRetries) {
                    std::cerr << "Max retries reached, giving up" << std::endl;
                    return;  // Exit thread
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        
        if (!channel) {
            std::cerr << "Failed to open shared memory channel" << std::endl;
            return;
        }
        
        char buffer[BUFFER_SIZE];
        
        // Main receive loop
        while (g_running && !channel->shouldExit()) {
            try {
                // Wait for data with timeout
                if (channel->r_wait(100)) {
                    // Get data from shared memory
                    size_t dataSize = channel->getDataSize();
                    if (dataSize > 0 && dataSize < BUFFER_SIZE) {
                        // Copy data from shared memory to local buffer
                        std::memcpy(buffer, channel->getPtr(), dataSize);
                        buffer[dataSize] = '\0'; // Ensure null termination for string data
                        
                        // Add message to queue
                        std::string message(buffer);
                        g_messageQueue.push(message);
                        
                        // Call the callback if set
                        std::lock_guard<std::mutex> lock(g_callbackMutex);
                        if (g_messageCallback) {
                            g_messageCallback(message);
                        }
                    }
                    
                    // Signal that we're done processing
                    channel->r_release();
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Error in receiver thread: " << e.what() << std::endl;
                break;
            }
            
            // Check if shared memory still exists
            if (!checkSharedMemoryExists(CHANNEL_NAME)) {
                std::cout << "Shared memory no longer exists, exiting receiver thread" << std::endl;
                break;
            }
            
            // Sleep to avoid high CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Clean up
        if (channel) {
            delete channel;
        }
        
        std::cout << "Receiver thread exiting" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error in receiver thread: " << e.what() << std::endl;
        if (channel) {
            delete channel;
        }
    }
}

// Node.js addon functions

// Start the receiver thread
Napi::Value StartReceiver(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (g_running) {
        return Napi::Boolean::New(env, false);
    }
    
    g_running = true;
    g_receiverThread = std::thread(receiverThreadFunc);
    
    return Napi::Boolean::New(env, true);
}

// Stop the receiver thread
Napi::Value StopReceiver(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (!g_running) {
        return Napi::Boolean::New(env, false);
    }
    
    g_running = false;
    
    if (g_receiverThread.joinable()) {
        g_receiverThread.join();
    }
    
    return Napi::Boolean::New(env, true);
}

// Get the next message from the queue
Napi::Value GetNextMessage(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    std::string message;
    if (g_messageQueue.pop(message)) {
        return Napi::String::New(env, message);
    } else {
        return env.Null();
    }
}

// Set a callback function to be called when a message is received
Napi::Value SetMessageCallback(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    if (info.Length() < 1 || !info[0].IsFunction()) {
        Napi::TypeError::New(env, "Function expected").ThrowAsJavaScriptException();
        return env.Null();
    }
    
    Napi::Function callback = info[0].As<Napi::Function>();
    
    // Create a ThreadSafeFunction
    Napi::ThreadSafeFunction tsfn = Napi::ThreadSafeFunction::New(
        env,
        callback,
        "MessageCallback",
        0,
        1
    );
    
    // Set the callback
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    g_messageCallback = [tsfn](const std::string& message) {
        auto callback = [message](Napi::Env env, Napi::Function jsCallback) {
            jsCallback.Call({Napi::String::New(env, message)});
        };
        
        tsfn.BlockingCall(callback);
    };
    
    return Napi::Boolean::New(env, true);
}

// Initialize the addon
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("startReceiver", Napi::Function::New(env, StartReceiver));
    exports.Set("stopReceiver", Napi::Function::New(env, StopReceiver));
    exports.Set("getNextMessage", Napi::Function::New(env, GetNextMessage));
    exports.Set("setMessageCallback", Napi::Function::New(env, SetMessageCallback));
    
    return exports;
}

NODE_API_MODULE(shared_memory, Init) 