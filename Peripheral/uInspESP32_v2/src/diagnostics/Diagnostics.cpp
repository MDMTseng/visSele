// Diagnostics.cpp - Error logging and system diagnostics implementation (Stage S4.2)
#include "Diagnostics.hpp"
#include "main.hpp"
#include <cstdio>

// Static error history buffer and ring buffer
GEN_ERROR_CODE Diagnostics::errorBuffer_[ERROR_HISTORY_CAPACITY];
RingBuf<GEN_ERROR_CODE, uint8_t> Diagnostics::errorHistory_(errorBuffer_, ERROR_HISTORY_CAPACITY);

void Diagnostics::init() {
    errorHistory_.clear();
}

void Diagnostics::pushError(GEN_ERROR_CODE code) {
    GEN_ERROR_CODE* head = errorHistory_.getHead();
    if (head == nullptr) {
        // No space, consume tail to keep the latest errors
        errorHistory_.consumeTail();
        head = errorHistory_.getHead();
    }
    
    if (head != nullptr) {
        *head = code;
        errorHistory_.pushHead();
    }
}

int Diagnostics::getErrorCount() {
    return errorHistory_.size();
}

GEN_ERROR_CODE Diagnostics::getError(int index) {
    if (index < 0 || index >= errorHistory_.size()) {
        return GEN_ERROR_CODE::NOP;
    }
    
    GEN_ERROR_CODE* error = errorHistory_.getTail(index);
    return (error != nullptr) ? *error : GEN_ERROR_CODE::NOP;
}

void Diagnostics::clearErrors() {
    errorHistory_.clear();
}

void Diagnostics::exportErrorsToJson(JsonArray jsonArray) {
    for (int i = 0; i < errorHistory_.size(); i++) {
        GEN_ERROR_CODE* error = errorHistory_.getTail(i);
        if (error != nullptr) {
            jsonArray.add(static_cast<int>(*error));
        }
    }
}

bool Diagnostics::hasErrors() {
    return errorHistory_.size() > 0;
}

GEN_ERROR_CODE Diagnostics::getLatestError() {
    if (errorHistory_.size() == 0) {
        return GEN_ERROR_CODE::NOP;
    }
    
    GEN_ERROR_CODE* latest = errorHistory_.getHead();
    return (latest != nullptr) ? *latest : GEN_ERROR_CODE::NOP;
}

// Legacy compatibility function
void ERROR_LOG_PUSH(GEN_ERROR_CODE code) {
    Diagnostics::pushError(code);
}
