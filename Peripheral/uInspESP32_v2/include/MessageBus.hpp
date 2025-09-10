// MessageBus.hpp - Centralized message routing (Stage S4.1)
#pragma once
#include <stdint.h>
#include <string>
#include "RingBuf.hpp"

// Message types for unified routing
enum class MessageType {
    TRIGGER_INFO = 1000,
    BRIEF_TRIGGER_INFO = 1005,
    SYSTEM_INFO = 1006,
    EXT_LOG = 1001,
    RESP_FRAME = 1002
};

// Unified message structure
struct Message {
    MessageType type;
    
    // Trigger info fields
    std::string camera_id;
    std::string trig_tag;
    int btrig_idx;
    int64_t trig_time_us;
    int trig_id;
    
    // Log field
    std::string log;
    
    // Response frame fields
    bool isAck;
    int resp_id;
    
    // Constructor for easy message creation
    Message() : type(MessageType::EXT_LOG), btrig_idx(0), trig_time_us(0), 
                trig_id(0), isAck(false), resp_id(0) {}
    
    // Factory methods for common message types
    static Message createTriggerInfo(const std::string& cameraId, const std::string& tag, 
                                   int64_t timeUs, int trigId) {
        Message msg;
        msg.type = MessageType::TRIGGER_INFO;
        msg.camera_id = cameraId;
        msg.trig_tag = tag;
        msg.trig_time_us = timeUs;
        msg.trig_id = trigId;
        return msg;
    }
    
    static Message createBriefTriggerInfo(int idx, int64_t timeUs, int trigId) {
        Message msg;
        msg.type = MessageType::BRIEF_TRIGGER_INFO;
        msg.btrig_idx = idx;
        msg.trig_time_us = timeUs;
        msg.trig_id = trigId;
        return msg;
    }
    
    static Message createSystemInfo() {
        Message msg;
        msg.type = MessageType::SYSTEM_INFO;
        return msg;
    }
    
    static Message createLog(const std::string& logMsg) {
        Message msg;
        msg.type = MessageType::EXT_LOG;
        msg.log = logMsg;
        return msg;
    }
    
    static Message createResponseFrame(bool ack, int respId) {
        Message msg;
        msg.type = MessageType::RESP_FRAME;
        msg.isAck = ack;
        msg.resp_id = respId;
        return msg;
    }
};

// MessageBus class for centralized message routing
class MessageBus {
public:
    static constexpr int QUEUE_CAPACITY = 20;
    
    // Initialize message bus
    static void init();
    
    // Send a message to the main communication queue
    static bool sendMessage(const Message& msg);
    
    // Send a message to the auxiliary queue
    static bool sendAuxMessage(const Message& msg);
    
    // Check if main queue has messages
    static bool hasMainMessages();
    
    // Check if aux queue has messages
    static bool hasAuxMessages();
    
    // Get next message from main queue (returns false if empty)
    static bool getNextMainMessage(Message& msg);
    
    // Get next message from aux queue (returns false if empty)
    static bool getNextAuxMessage(Message& msg);
    
    // Get queue sizes
    static int getMainQueueSize();
    static int getAuxQueueSize();
    
    // Clear all queues
    static void clearAll();
    
private:
    // Internal queue structures (wrapping existing RingBuf)
    static RingBuf_Static<Message, QUEUE_CAPACITY, uint8_t> mainQueue_;
    static RingBuf_Static<Message, QUEUE_CAPACITY, uint8_t> auxQueue_;
    
    // Convert between old and new message formats (internal use only)
    static Message convertFromTaskQ2CommInfo(const struct TaskQ2CommInfo& oldMsg);
    static struct TaskQ2CommInfo convertToTaskQ2CommInfo(const Message& newMsg);
};

// Legacy compatibility - these will be removed in later stages
extern RingBuf_Static<struct TaskQ2CommInfo, 20, uint8_t> TaskQ2CommInfoQ;
extern RingBuf_Static<struct TaskQ2CommInfo, 20, uint8_t> AUX2CommInfoQ;
