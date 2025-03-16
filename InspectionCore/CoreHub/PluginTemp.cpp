#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <atomic>
#include <functional>

// Platform detection
#if defined(_WIN32) || defined(__MINGW32__) || defined(__MINGW64__) || defined(__CYGWIN__)
#define IS_WINDOWS 1
#include <windows.h>
#include <tlhelp32.h>
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

// Global variables for signal handling
volatile sig_atomic_t g_running = 1;
pid_t g_senderPid = -1;  // Store the sender's PID for monitoring

// Signal handler
void signal_handler(int sig) {
    g_running = 0;
}

// Function to check if a process is still running (cross-platform)
bool is_process_running(pid_t pid) {
    if (pid <= 0) return false;
    
#if IS_WINDOWS
    // Windows implementation
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        return false;  // Process not found or cannot be accessed
    }
    
    DWORD exitCode;
    bool isRunning = GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(hProcess);
    return isRunning;
#else
    // Unix implementation
    return kill(pid, 0) == 0;
#endif
}

// Alternative method to check if a process exists using shared memory heartbeat
class ProcessHeartbeat {
private:
    std::string m_name;
    boost::interprocess::shared_memory_object* m_shm;
    boost::interprocess::mapped_region* m_region;
    time_t* m_lastHeartbeat;
    bool m_isCreator;
    
public:
    ProcessHeartbeat(const std::string& name, bool isCreator) 
        : m_name(name + "_heartbeat"), 
          m_shm(nullptr), 
          m_region(nullptr), 
          m_lastHeartbeat(nullptr),
          m_isCreator(isCreator) {
        
        try {
            if (isCreator) {
                // Remove any existing shared memory
                boost::interprocess::shared_memory_object::remove(m_name.c_str());
                
                // Create shared memory
                m_shm = new boost::interprocess::shared_memory_object(
                    boost::interprocess::create_only,
                    m_name.c_str(),
                    boost::interprocess::read_write
                );
                
                // Set size
                m_shm->truncate(sizeof(time_t));
            } else {
                // Open existing shared memory
                m_shm = new boost::interprocess::shared_memory_object(
                    boost::interprocess::open_only,
                    m_name.c_str(),
                    boost::interprocess::read_write
                );
            }
            
            // Map the shared memory
            m_region = new boost::interprocess::mapped_region(
                *m_shm,
                boost::interprocess::read_write
            );
            
            // Get pointer to the heartbeat timestamp
            m_lastHeartbeat = static_cast<time_t*>(m_region->get_address());
            
            if (isCreator) {
                // Initialize the heartbeat timestamp
                *m_lastHeartbeat = time(nullptr);
            }
        } catch (const std::exception& e) {
            std::cerr << "ProcessHeartbeat initialization error: " << e.what() << std::endl;
            cleanup();
            throw;
        }
    }
    
    ~ProcessHeartbeat() {
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
    }
    
    // Update the heartbeat timestamp (called by the process being monitored)
    void update() {
        if (m_lastHeartbeat) {
            *m_lastHeartbeat = time(nullptr);
            if (m_region) {
                m_region->flush();
            }
        }
    }
    
    // Check if the heartbeat is recent (called by the monitoring process)
    bool isAlive(int timeoutSeconds = 30) {
        if (!m_lastHeartbeat) return false;
        
        time_t now = time(nullptr);
        return (now - *m_lastHeartbeat) < timeoutSeconds;
    }
};

// Simplified shared memory channel class
struct SharedMemoryData {
    SharedMemoryData() : sender_ready(1), receiver_ready(0), complete_signal(0), data_size(0) {}
    
    boost::interprocess::interprocess_semaphore sender_ready;
    boost::interprocess::interprocess_semaphore receiver_ready;
    boost::interprocess::interprocess_semaphore complete_signal;
    
    size_t data_size;
    // Data buffer follows this structure in memory
};

// Simplified shared memory channel class with the requested interface
class smem_channel {
private:
    std::string m_name;
    size_t m_memSize;
    bool m_isCreator;
    bool m_shouldExit;
    
    boost::interprocess::shared_memory_object* m_shm;
    boost::interprocess::mapped_region* m_region;
    SharedMemoryData* m_data;
    char* m_buffer;
    
    // Named semaphores for Windows
    boost::interprocess::named_semaphore* m_senderReadySem;
    boost::interprocess::named_semaphore* m_receiverReadySem;
    boost::interprocess::named_semaphore* m_completeSem;
    bool m_useNamedSemaphores;
    
public:
    // Constructor with option to create the shared memory
    smem_channel(const std::string name, size_t memSize, bool isCreator) 
        : m_name(name),
          m_memSize(memSize),
          m_isCreator(isCreator),
          m_shm(nullptr), 
          m_region(nullptr), 
          m_data(nullptr),
          m_buffer(nullptr),
          m_senderReadySem(nullptr),
          m_receiverReadySem(nullptr),
          m_completeSem(nullptr),
          m_useNamedSemaphores(IS_WINDOWS),
          m_shouldExit(false) {
        
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
    void r_wait() {
        try {
            std::cout << "Receiver waiting for data... (channel: " << m_name << ")" << std::endl;
            
            // Check if we should exit before waiting
            if (m_shouldExit) {
                std::cout << "Receiver exiting without waiting (channel: " << m_name << ")" << std::endl;
                return;
            }
            
            if (m_useNamedSemaphores) {
                // For Windows, use named semaphores
                // Use a polling approach with try_wait
                while (!m_shouldExit) {
                    if (m_receiverReadySem->try_wait()) {
                        break;  // Successfully acquired the semaphore
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                
                if (m_shouldExit) {
                    std::cout << "Receiver interrupted while waiting (named semaphore: " << m_name << ")" << std::endl;
                    return;
                }
                std::cout << "Receiver woke up (named semaphore: " << m_name << ")" << std::endl;
            } else {
                // For non-Windows, use interprocess semaphores
                // Use a polling approach to allow for interruption
                while (!m_shouldExit) {
                    try {
                        if (m_data->receiver_ready.try_wait()) {
                            break;  // Successfully acquired the semaphore
                        }
                    } catch (...) {
                        // Ignore exceptions from try_wait
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                
                if (m_shouldExit) {
                    std::cout << "Receiver interrupted while waiting (interprocess semaphore: " << m_name << ")" << std::endl;
                    return;
                }
                std::cout << "Receiver woke up (interprocess semaphore: " << m_name << ")" << std::endl;
            }
            
            // Ensure memory is synchronized after waking up
            if (m_region) {
                m_region->flush();
            }
        } catch (const std::exception& e) {
            std::cerr << "r_wait error: " << e.what() << std::endl;
            throw;
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
                std::cout << "Receiver signaled completion (named semaphore: " << m_name << ")" << std::endl;
            } else {
                m_data->complete_signal.post();
                std::cout << "Receiver signaled completion (interprocess semaphore: " << m_name << ")" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "r_release error: " << e.what() << std::endl;
            throw;
        }
    }
    
    // Sender posts data is ready
    void s_post() {
        try {
            // Make sure data is fully written to shared memory before signaling
            if (m_region) {
                m_region->flush();  // Ensure memory writes are visible to other processes
            }
            
            if (m_useNamedSemaphores) {
                // For Windows, use named semaphores
                m_receiverReadySem->post();
                std::cout << "Sender posted data is ready (named semaphore: " << m_name << ")" << std::endl;
            } else {
                // For non-Windows, use interprocess semaphores
                m_data->receiver_ready.post();
                std::cout << "Sender posted data is ready (interprocess semaphore: " << m_name << ")" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "s_post error: " << e.what() << std::endl;
            throw;
        }
    }
    
    // Sender waits for receiver to complete processing
    void s_wait_remote() {
        try {
            std::cout << "Sender waiting for receiver to complete... (channel: " << m_name << ")" << std::endl;
            
            // Check if we should exit before waiting
            if (m_shouldExit) {
                std::cout << "Sender exiting without waiting for completion (channel: " << m_name << ")" << std::endl;
                return;
            }
            
            if (m_useNamedSemaphores) {
                // Use a polling approach with try_wait
                while (!m_shouldExit) {
                    if (m_completeSem->try_wait()) {
                        break;  // Successfully acquired the semaphore
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                
                if (m_shouldExit) {
                    std::cout << "Sender interrupted while waiting for completion (named semaphore: " << m_name << ")" << std::endl;
                    return;
                }
                std::cout << "Sender woke up after completion (named semaphore: " << m_name << ")" << std::endl;
            } else {
                // Use a polling approach to allow for interruption
                while (!m_shouldExit) {
                    try {
                        if (m_data->complete_signal.try_wait()) {
                            break;  // Successfully acquired the semaphore
                        }
                    } catch (...) {
                        // Ignore exceptions from try_wait
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                
                if (m_shouldExit) {
                    std::cout << "Sender interrupted while waiting for completion (interprocess semaphore: " << m_name << ")" << std::endl;
                    return;
                }
                std::cout << "Sender woke up after completion (interprocess semaphore: " << m_name << ")" << std::endl;
            }
            
            // Reset sender ready for next send
            if (m_useNamedSemaphores) {
                m_senderReadySem->post();
            } else {
                m_data->sender_ready.post();
            }
            
            // Ensure memory is synchronized
            if (m_region) {
                m_region->flush();
            }
        } catch (const std::exception& e) {
            std::cerr << "s_wait_remote error: " << e.what() << std::endl;
            throw;
        }
    }
    
    // Helper method to set data size
    void setDataSize(size_t size) {
        if (size <= m_memSize) {
            m_data->data_size = size;
        } else {
            throw std::runtime_error("Data size exceeds buffer size");
        }
    }
    
    // Helper method to get data size
    size_t getDataSize() const {
        return m_data->data_size;
    }
    
    // Signal that the channel should exit
    void signalExit() {
        m_shouldExit = true;
        
        // Wake up any waiting threads
        if (m_useNamedSemaphores) {
            if (m_receiverReadySem) m_receiverReadySem->post();
            if (m_completeSem) m_completeSem->post();
            if (m_senderReadySem) m_senderReadySem->post();
        } else {
            if (m_data) {
                m_data->receiver_ready.post();
                m_data->complete_signal.post();
                m_data->sender_ready.post();
            }
        }
    }
};

// Utility class to monitor the existence of a named shared memory object
class SharedMemoryMonitor {
private:
    std::string m_name;
    bool m_exists;
    std::thread m_monitorThread;
    std::atomic<bool> m_running;
    std::function<void()> m_onRemoveCallback;
    
public:
    SharedMemoryMonitor(const std::string& name, std::function<void()> onRemoveCallback = nullptr)
        : m_name(name),
          m_exists(true),
          m_running(true),
          m_onRemoveCallback(onRemoveCallback) {
        
        // Start monitoring thread
        m_monitorThread = std::thread([this]() {
            while (m_running) {
                bool currentlyExists = checkExists();
                std::cout << "SharedMemoryMonitor: Shared memory '" << m_name << "' currently exists: " << currentlyExists << std::endl;
                // If state changed from exists to doesn't exist
                if (m_exists && !currentlyExists) {
                    m_exists = false;
                    std::cout << "SharedMemoryMonitor: Shared memory '" << m_name << "' has been removed" << std::endl;
                    
                    // Call the callback if provided
                    if (m_onRemoveCallback) {
                        m_onRemoveCallback();
                    }
                }
                // If state changed from doesn't exist to exists
                else if (!m_exists && currentlyExists) {
                    m_exists = true;
                    std::cout << "SharedMemoryMonitor: Shared memory '" << m_name << "' now exists" << std::endl;
                }
                
                // Check every 100ms
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }
    
    ~SharedMemoryMonitor() {
        m_running = false;
        if (m_monitorThread.joinable()) {
            m_monitorThread.join();
        }
    }
    
    bool exists() const {
        return m_exists;
    }
    
private:
    bool checkExists() {
        try {
            // Try to open the shared memory (read-only is sufficient for checking existence)
            boost::interprocess::shared_memory_object shm(
                boost::interprocess::open_only,
                m_name.c_str(),
                boost::interprocess::read_only
            );
            
            // If we get here, the shared memory exists
            return true;
        }
        catch (const boost::interprocess::interprocess_exception& ex) {
            // Check if the error is specifically about the shared memory not existing
            if (ex.get_error_code() == boost::interprocess::not_found_error) {
                return false;
            }
            
            // For other errors, log but assume it might still exist
            std::cerr << "Error checking shared memory existence: " << ex.what() << std::endl;
            return true;
        }
    }
};

// Function to handle receiving messages
void RECV() {
    std::cout << "RECV process started (PID: " << getpid() << ")" << std::endl;
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create the receiver channel
    smem_channel* channel = nullptr;
    // ProcessHeartbeat* heartbeatMonitor = nullptr;
    SharedMemoryMonitor* sharedMemMonitor = nullptr;
    
    try {
        // Try multiple times to open the channel
        int retries = 0;
        while (retries < 5 && g_running) {
            try {
                std::cout << "RECV process attempting to open channel (attempt " << retries+1 << ")" << std::endl;
                channel = new smem_channel(CHANNEL_NAME, BUFFER_SIZE, false);
                
                // Try to open the heartbeat monitor
                // heartbeatMonitor = new ProcessHeartbeat(CHANNEL_NAME, false);
                
                // Create a shared memory monitor to detect when the shared memory is removed
                sharedMemMonitor = new SharedMemoryMonitor(CHANNEL_NAME, [&]() {
                    std::cout << "RECV detected that shared memory has been removed" << std::endl;
                    if (channel) {
                        channel->signalExit();
                    }
                    g_running = 0;
                });
                
                break;  // Success
            } catch (const std::exception& e) {
                std::cerr << "Failed to open channel: " << e.what() << std::endl;
                retries++;
                if (retries >= 5) throw;  // Re-throw after max retries
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        
        std::cout << "RECV channel initialized" << std::endl;
        
        char buffer[BUFFER_SIZE];
        
        // Main receive loop
        while (g_running) {
            try {
                // Start a monitoring thread to check if sender is still alive
                std::thread monitor_thread([&]() {
                    while (g_running) {
                        bool senderAlive = false;
                        
                        // First check if the shared memory still exists
                        if (sharedMemMonitor && !sharedMemMonitor->exists()) {
                            std::cout << "RECV detected that shared memory no longer exists" << std::endl;
                            senderAlive = false;
                        }
                        // Then try the heartbeat method
                        // else if (heartbeatMonitor && !heartbeatMonitor->isAlive()) {
                        //     std::cout << "RECV detected that sender process has stopped sending heartbeats" << std::endl;
                        //     senderAlive = false;
                        // } 
                        // Then try the process ID method as fallback
                        else if (g_senderPid > 0 && !is_process_running(g_senderPid)) {
                            std::cout << "RECV detected that sender process (PID: " << g_senderPid << ") has exited" << std::endl;
                            senderAlive = false;
                        }
                        else {
                            senderAlive = true;
                        }
                        
                        if (!senderAlive) {
                            // Signal the channel to exit
                            if (channel) {
                                channel->signalExit();
                            }
                            g_running = 0;
                            break;
                        }
                        
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                });
                
                // Detach the thread so it runs independently
                monitor_thread.detach();
                
                // Wait for data
                channel->r_wait();
                
                if (!g_running) break;
                
                // Get data from shared memory
                size_t dataSize = channel->getDataSize();
                if (dataSize > 0 && dataSize < BUFFER_SIZE) {
                    // Copy data from shared memory to local buffer
                    std::memcpy(buffer, channel->getPtr(), dataSize);
                    buffer[dataSize] = '\0'; // Ensure null termination for string data
                    
                    // Process the data (just print it)
                    std::cout << "RECV process received message: " << buffer << std::endl;
                    
                    // If we receive "quit", exit the loop
                    // if (strcmp(buffer, "quit") == 0) {
                    //     std::cout << "RECV process received quit command" << std::endl;
                    //     g_running = 0;
                    // }
                }
                
                // Signal that we're done processing
                channel->r_release();
            }
            catch (const std::exception& e) {
                std::cerr << "Error in RECV process: " << e.what() << std::endl;
                break;
            }
            
            // Sleep to avoid high CPU usage if there's an error
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Clean up
        if (channel) {
            delete channel;
        }
        
        // if (heartbeatMonitor) {
        //     delete heartbeatMonitor;
        // }
        
        if (sharedMemMonitor) {
            delete sharedMemMonitor;
        }
        
        std::cout << "RECV process exiting" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error in RECV process: " << e.what() << std::endl;
        if (channel) {
            delete channel;
        }
        // if (heartbeatMonitor) {
        //     delete heartbeatMonitor;
        // }
        if (sharedMemMonitor) {
            delete sharedMemMonitor;
        }
        exit(1);
    }
    
    exit(0);
}

// Function to handle sending messages
void SEND() {
    std::cout << "SEND process started (PID: " << getpid() << ")" << std::endl;
    
    // Store the sender PID for monitoring
    g_senderPid = getpid();
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    
    // Create the sender channel
    smem_channel* channel = nullptr;
    ProcessHeartbeat* heartbeat = nullptr;
    
    try {
        // Create the channel
        channel = new smem_channel(CHANNEL_NAME, BUFFER_SIZE, true);
        
        // Create the heartbeat
        heartbeat = new ProcessHeartbeat(CHANNEL_NAME, true);
        
        std::cout << "SEND channel initialized" << std::endl;
        
        // Start a heartbeat thread
        std::thread heartbeat_thread([&]() {
            while (g_running) {
                if (heartbeat) {
                    heartbeat->update();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        });
        
        // Detach the thread so it runs independently
        heartbeat_thread.detach();
        
        // Give the receiver process time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        std::cout << "Ready to send messages. Type a message or 'quit' to exit." << std::endl;
        
        // Main send loop
        std::string input;
        while (g_running) {
            std::getline(std::cin, input);
            
            if (input == "quit" || !g_running) {
                // Send quit message to receiver
                std::string quitMsg = "quit";
                void* sharedMemPtr = channel->getPtr();
                size_t messageSize = quitMsg.length() + 1;
                std::memcpy(sharedMemPtr, quitMsg.c_str(), messageSize);
                channel->setDataSize(messageSize);
                channel->s_post();
                channel->s_wait_remote();
                break;
            }
            
            // Use "Hello World" if input is empty
            std::string message = input.empty() ? "Hello World" : input;
            
            try {
                // Copy message to shared memory
                void* sharedMemPtr = channel->getPtr();
                size_t messageSize = message.length() + 1; // Include null terminator
                
                if (messageSize > channel->size()) {
                    std::cerr << "Message too large for shared memory buffer" << std::endl;
                    continue;
                }
                
                // Copy the message to shared memory
                std::memcpy(sharedMemPtr, message.c_str(), messageSize);
                channel->setDataSize(messageSize);
                std::cout << "Sent data size: " << messageSize << std::endl;
                
                // Signal that data is ready
                channel->s_post();
                
                // Wait for receiver to process the data
                std::cout << "Waiting for receiver to process the data..." << std::endl;
                channel->s_wait_remote();
                std::cout << "Receiver processed the data" << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "Error sending message: " << e.what() << std::endl;
            }
        }
        
        // Clean up
        if (channel) {
            delete channel;
        }
        
        if (heartbeat) {
            delete heartbeat;
        }
        
        std::cout << "SEND process exiting" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error in SEND process: " << e.what() << std::endl;
        if (channel) {
            delete channel;
        }
        if (heartbeat) {
            delete heartbeat;
        }
        return;
    }
}

int main(int argc, char** argv) {
    std::cout << "Simple shared memory IPC test using fork()" << std::endl;
    
    // Parse command line arguments
    bool isSender = true;  // Default to sender mode
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--recv") {
            isSender = false;
        } else if (arg == "--send") {
            isSender = true;
        } else if (arg == "--sender-pid" && i + 1 < argc) {
            // Allow specifying the sender PID for monitoring
            g_senderPid = std::stoi(argv[++i]);
            std::cout << "Will monitor sender process with PID: " << g_senderPid << std::endl;
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --send         Run as sender (default)" << std::endl;
            std::cout << "  --recv         Run as receiver" << std::endl;
            std::cout << "  --sender-pid N Set the sender PID to monitor (for receiver mode)" << std::endl;
            std::cout << "  --help         Show this help message" << std::endl;
            return 0;
        }
    }
    
    // If no arguments provided, fork a child process for the receiver
    if (argc == 1) {
#if IS_WINDOWS
        std::cout << "Fork not supported on Windows. Please run sender and receiver separately." << std::endl;
        std::cout << "Run this program with --send in one terminal and --recv in another." << std::endl;
        return 1;
#else
        // Fork a child process to handle receiving
        pid_t childPid = fork();
        
        if (childPid < 0) {
            // Fork failed
            std::cerr << "Failed to fork child process" << std::endl;
            return 1;
        }
        else if (childPid == 0) {
            // This is the child process - run as receiver
            // Set the parent's PID as the sender to monitor
            g_senderPid = getppid();
            std::cout << "Child will monitor parent process with PID: " << g_senderPid << std::endl;
            
            RECV();
            // Child process should not return from RECV()
            return 0;
        }
        else {
            // This is the parent process - run as sender
            std::cout << "Parent process started child with PID: " << childPid << std::endl;
            
            // Run the sender
            SEND();
            
            // Wait for the child process to exit
            // int status;
            // std::cout << "Waiting for child process to exit..." << std::endl;
            // waitpid(childPid, &status, 0);
            // std::cout << "Child process exited with status: " << WEXITSTATUS(status) << std::endl;
        }
#endif
    }
    else {
        // Run in the specified mode
        if (isSender) {
            SEND();
        } else {
            RECV();
        }
    }
    
    return 0;
} 