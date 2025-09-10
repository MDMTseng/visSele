// MessageBus.cpp - Centralized message routing implementation (Stage S4.1)
#include "MessageBus.hpp"
#include "main.hpp"
#include <cstring>

// Legacy structures for backward compatibility
enum TaskQ2CommInfo_Type {
    trigInfo = 1000,
    btrigInfo = 1005,
    systemInfo = 1006,
    ext_log = 1001,
    respFrame = 1002,
};

struct TaskQ2CommInfo {
    TaskQ2CommInfo_Type type;
    std::string camera_id;
    std::string trig_tag;
    int btrig_idx;
    int64_t trig_time_us;
    int trig_id;
    std::string log;
    bool isAck;
    int resp_id;
};

// Static queue instances
RingBuf_Static<Message, MessageBus::QUEUE_CAPACITY, uint8_t> MessageBus::mainQueue_;
RingBuf_Static<Message, MessageBus::QUEUE_CAPACITY, uint8_t> MessageBus::auxQueue_;

// Legacy queues for backward compatibility (will be removed in later stages)
RingBuf_Static<struct TaskQ2CommInfo, 20, uint8_t> TaskQ2CommInfoQ;
RingBuf_Static<struct TaskQ2CommInfo, 20, uint8_t> AUX2CommInfoQ;

void MessageBus::init() {
    // Initialize queues
    mainQueue_.clear();
    auxQueue_.clear();
    
    // Initialize legacy queues for backward compatibility
    TaskQ2CommInfoQ.clear();
    AUX2CommInfoQ.clear();
}

bool MessageBus::sendMessage(const Message& msg) {
    Message* head = mainQueue_.getHead();
    if (head == nullptr) {
        // No space, consume tail to make room
        mainQueue_.consumeTail();
        head = mainQueue_.getHead();
    }
    
    if (head != nullptr) {
        *head = msg;
        mainQueue_.pushHead();
        
        // Also send to legacy queue for backward compatibility
        TaskQ2CommInfo* legacyHead = TaskQ2CommInfoQ.getHead();
        if (legacyHead != nullptr) {
            *legacyHead = convertToTaskQ2CommInfo(msg);
            TaskQ2CommInfoQ.pushHead();
        }
        
        return true;
    }
    
    return false;
}

bool MessageBus::sendAuxMessage(const Message& msg) {
    Message* head = auxQueue_.getHead();
    if (head == nullptr) {
        // No space, consume tail to make room
        auxQueue_.consumeTail();
        head = auxQueue_.getHead();
    }
    
    if (head != nullptr) {
        *head = msg;
        auxQueue_.pushHead();
        
        // Also send to legacy queue for backward compatibility
        TaskQ2CommInfo* legacyHead = AUX2CommInfoQ.getHead();
        if (legacyHead != nullptr) {
            *legacyHead = convertToTaskQ2CommInfo(msg);
            AUX2CommInfoQ.pushHead();
        }
        
        return true;
    }
    
    return false;
}

bool MessageBus::hasMainMessages() {
    return mainQueue_.size() > 0;
}

bool MessageBus::hasAuxMessages() {
    return auxQueue_.size() > 0;
}

bool MessageBus::getNextMainMessage(Message& msg) {
    if (mainQueue_.size() == 0) {
        return false;
    }
    
    msg = *mainQueue_.getTail();
    mainQueue_.consumeTail();
    return true;
}

bool MessageBus::getNextAuxMessage(Message& msg) {
    if (auxQueue_.size() == 0) {
        return false;
    }
    
    msg = *auxQueue_.getTail();
    auxQueue_.consumeTail();
    return true;
}

int MessageBus::getMainQueueSize() {
    return mainQueue_.size();
}

int MessageBus::getAuxQueueSize() {
    return auxQueue_.size();
}

void MessageBus::clearAll() {
    mainQueue_.clear();
    auxQueue_.clear();
    TaskQ2CommInfoQ.clear();
    AUX2CommInfoQ.clear();
}

Message MessageBus::convertFromTaskQ2CommInfo(const struct TaskQ2CommInfo& oldMsg) {
    Message newMsg;
    
    // Convert type
    switch (oldMsg.type) {
        case trigInfo:
            newMsg.type = MessageType::TRIGGER_INFO;
            break;
        case btrigInfo:
            newMsg.type = MessageType::BRIEF_TRIGGER_INFO;
            break;
        case systemInfo:
            newMsg.type = MessageType::SYSTEM_INFO;
            break;
        case ext_log:
            newMsg.type = MessageType::EXT_LOG;
            break;
        case respFrame:
            newMsg.type = MessageType::RESP_FRAME;
            break;
        default:
            newMsg.type = MessageType::EXT_LOG;
            break;
    }
    
    // Copy fields
    newMsg.camera_id = oldMsg.camera_id;
    newMsg.trig_tag = oldMsg.trig_tag;
    newMsg.btrig_idx = oldMsg.btrig_idx;
    newMsg.trig_time_us = oldMsg.trig_time_us;
    newMsg.trig_id = oldMsg.trig_id;
    newMsg.log = oldMsg.log;
    newMsg.isAck = oldMsg.isAck;
    newMsg.resp_id = oldMsg.resp_id;
    
    return newMsg;
}

struct TaskQ2CommInfo MessageBus::convertToTaskQ2CommInfo(const Message& newMsg) {
    struct TaskQ2CommInfo oldMsg;
    
    // Convert type
    switch (newMsg.type) {
        case MessageType::TRIGGER_INFO:
            oldMsg.type = trigInfo;
            break;
        case MessageType::BRIEF_TRIGGER_INFO:
            oldMsg.type = btrigInfo;
            break;
        case MessageType::SYSTEM_INFO:
            oldMsg.type = systemInfo;
            break;
        case MessageType::EXT_LOG:
            oldMsg.type = ext_log;
            break;
        case MessageType::RESP_FRAME:
            oldMsg.type = respFrame;
            break;
        default:
            oldMsg.type = ext_log;
            break;
    }
    
    // Copy fields
    oldMsg.camera_id = newMsg.camera_id;
    oldMsg.trig_tag = newMsg.trig_tag;
    oldMsg.btrig_idx = newMsg.btrig_idx;
    oldMsg.trig_time_us = newMsg.trig_time_us;
    oldMsg.trig_id = newMsg.trig_id;
    oldMsg.log = newMsg.log;
    oldMsg.isAck = newMsg.isAck;
    oldMsg.resp_id = newMsg.resp_id;
    
    return oldMsg;
}
