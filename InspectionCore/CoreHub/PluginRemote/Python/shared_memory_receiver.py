#!/usr/bin/env python3

import mmap
import time
import struct
import os
import signal
import sys
import posix_ipc
import ctypes

class SharedMemoryData(ctypes.Structure):
    """Match the C++ SharedMemoryData structure"""
    _fields_ = [
        ("data_size", ctypes.c_size_t),
    ]

class SharedMemoryReceiver:
    def __init__(self, channel_name="TestChannel", size=4096):
        self.channel_name = channel_name
        self.size = size
        self.running = True
        self.shm = None
        self.mapped_mem = None
        
        # Initialize semaphores with the same names as in C++
        try:
            self.sender_ready_sem = posix_ipc.Semaphore(f"{channel_name}_sender", posix_ipc.O_CREAT)
            self.receiver_ready_sem = posix_ipc.Semaphore(f"{channel_name}_receiver", posix_ipc.O_CREAT)
            self.complete_sem = posix_ipc.Semaphore(f"{channel_name}_complete", posix_ipc.O_CREAT)
        except posix_ipc.ExistentialError:
            # If semaphores already exist, open them
            self.sender_ready_sem = posix_ipc.Semaphore(f"{channel_name}_sender")
            self.receiver_ready_sem = posix_ipc.Semaphore(f"{channel_name}_receiver")
            self.complete_sem = posix_ipc.Semaphore(f"{channel_name}_complete")
        
        # Set up signal handler for graceful shutdown
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)

    def signal_handler(self, signum, frame):
        print("\nReceived signal to terminate")
        self.cleanup()
        self.running = False

    def cleanup(self):
        """Clean up resources"""
        if self.mapped_mem:
            self.mapped_mem.close()
        if self.shm:
            self.shm.close_fd()
        
        try:
            # Clean up semaphores
            self.sender_ready_sem.close()
            self.receiver_ready_sem.close()
            self.complete_sem.close()
        except:
            pass

    def start_receiving(self):
        try:
            # Open shared memory
            self.shm = posix_ipc.SharedMemory(self.channel_name)
            print(f"Opened shared memory: {self.channel_name}")
            
            # Memory map the shared memory segment
            self.mapped_mem = mmap.mmap(self.shm.fd, self.size, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
            print("Memory mapped successfully")

            # Calculate header size
            header_size = ctypes.sizeof(SharedMemoryData)

            print("Starting to receive data...")
            while self.running:
                try:
                    # Wait for data to be ready (with timeout to allow checking running flag)
                    while self.running:
                        try:
                            if self.receiver_ready_sem.acquire(0.1):  # 100ms timeout
                                break
                        except posix_ipc.BusyError:
                            continue
                    
                    if not self.running:
                        break

                    # Read the header to get data size
                    self.mapped_mem.seek(0)
                    header_bytes = self.mapped_mem.read(header_size)
                    header = SharedMemoryData.from_buffer_copy(header_bytes)
                    
                    # Read the actual data
                    data = self.mapped_mem.read(header.data_size)
                    
                    # Try to decode the data
                    try:
                        decoded_data = data.decode('utf-8').strip('\x00')
                        if decoded_data:
                            print(f"Received: {decoded_data}")
                            
                            # Check for quit command
                            if decoded_data == "quit":
                                print("Received quit command")
                                self.running = False
                    except UnicodeDecodeError:
                        print(f"Received raw data: {data}")

                    # Signal completion
                    self.complete_sem.release()
                    print("Signaled completion to sender")

                except Exception as e:
                    print(f"Error during receive cycle: {e}")
                    break

        except Exception as e:
            print(f"Error: {e}")
        finally:
            self.cleanup()

if __name__ == "__main__":
    # Allow custom shared memory name from command line
    channel_name = sys.argv[1] if len(sys.argv) > 1 else "TestChannel"
    
    receiver = SharedMemoryReceiver(channel_name=channel_name)
    print(f"Starting shared memory receiver for '{channel_name}'")
    receiver.start_receiving() 