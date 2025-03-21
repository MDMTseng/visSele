#ifndef CJSONPP_H
#define CJSONPP_H

#include <string>
#include <iostream>
#include "contrib/cJSON/cJSON.h"

/**
 * cJSONPP - A simplified C++ wrapper for cJSON with bracket notation access
 */
class cJSONPP {
private:
    cJSON* m_json;
    bool m_owned;

    // Helper class for accessing nested elements
    class ElementRef {
    protected:
        cJSON* m_parent;
        cJSON* m_element;
        bool m_is_array;
        int m_index;
        std::string m_key;
        bool m_read_only;

    public:
        // Constructor for object element
        ElementRef(cJSON* parent, const std::string& key, bool readOnly = true)
            : m_parent(parent), m_is_array(false), m_index(-1), m_key(key), m_read_only(readOnly) 
        {
            m_element = cJSON_GetObjectItem(parent, key.c_str());
            if (!m_element && !m_read_only) {
                m_element = cJSON_CreateNull();
                cJSON_AddItemToObject(parent, key.c_str(), m_element);
            }
        }

        // Constructor for array element
        ElementRef(cJSON* parent, int index, bool readOnly = true)
            : m_parent(parent), m_is_array(true), m_index(index), m_key(""), m_read_only(readOnly)
        {
            if (!m_read_only) {
                // Ensure array is large enough
                while (cJSON_GetArraySize(parent) <= index) {
                    cJSON_AddItemToArray(parent, cJSON_CreateNull());
                }
            }
            m_element = cJSON_GetArrayItem(parent, index);
        }
        
        // Special constructor for creating a read-only copy
        ElementRef(const ElementRef& other, bool forceReadOnly)
            : m_parent(other.m_parent), m_element(other.m_element), m_is_array(other.m_is_array),
              m_index(other.m_index), m_key(other.m_key), m_read_only(forceReadOnly || other.m_read_only)
        {
            // This simply copies all fields but forces read-only if requested
        }
        
        // Copy constructor - creates a read-only copy by default
        ElementRef(const ElementRef& other)
            : m_parent(other.m_parent), m_element(other.m_element), m_is_array(other.m_is_array),
              m_index(other.m_index), m_key(other.m_key), m_read_only(true)
        {
            // Default copy creates a read-only version
        }
        
        // Copy assignment operator - creates a read-only copy by default
        ElementRef& operator=(const ElementRef& other)
        {
            if (this != &other) {
                m_parent = other.m_parent;
                m_element = other.m_element;
                m_is_array = other.m_is_array;
                m_index = other.m_index;
                m_key = other.m_key;
                m_read_only = true; // Always make copy assignments read-only
            }
            return *this;
        }

        // Get the underlying cJSON element
        cJSON* get() const { return m_element; }

        // Check if element exists and has expected type
        bool exists() const { return m_element != nullptr; }
        bool isNumber() const { return m_element != nullptr && cJSON_IsNumber(m_element); }
        bool isString() const { return m_element != nullptr && cJSON_IsString(m_element); }
        bool isBool() const { return m_element != nullptr && (cJSON_IsTrue(m_element) || cJSON_IsFalse(m_element)); }
        bool isObject() const { return m_element != nullptr && cJSON_IsObject(m_element); }
        bool isArray() const { return m_element != nullptr && cJSON_IsArray(m_element); }
        bool isNull() const { return m_element != nullptr && cJSON_IsNull(m_element); }

        // Write-enabled version of this element
        ElementRef w() {
            if (!m_element) return ElementRef(nullptr, m_key, false);
            if (m_is_array) {
                return ElementRef(m_parent, m_index, false);
            } else {
                return ElementRef(m_parent, m_key, false);
            }
        }

        // Create a writable copy
        ElementRef writableCopy() const {
            return ElementRef(*this, false); // Use the special constructor to create a writable copy
        }

        // Read-only version of this element
        ElementRef readOnly() const {
            return ElementRef(*this, true); // Use the special constructor to create a read-only copy
        }

        // Safe access methods (read-only, won't modify structure)
        ElementRef safeAccess(const std::string& key) const {
            if (!exists() || !cJSON_IsObject(m_element)) {
                return ElementRef(nullptr, key, true);
            }
            return ElementRef(m_element, key, true);
        }

        ElementRef safeAccess(int index) const {
            if (!exists() || !cJSON_IsArray(m_element) || index >= cJSON_GetArraySize(m_element)) {
                return ElementRef(nullptr, index, true);
            }
            return ElementRef(m_element, index, true);
        }

        // Nested property access
        ElementRef operator[](const std::string& key) {
            if (m_read_only) {
                return safeAccess(key);
            }
            
            if (!cJSON_IsObject(m_element)) {
                // Replace current element with an object
                cJSON* newObj = cJSON_CreateObject();
                replace(newObj);
                m_element = newObj;
            }
            return ElementRef(m_element, key, m_read_only);
        }

        // Nested array access
        ElementRef operator[](int index) {
            if (m_read_only) {
                return safeAccess(index);
            }
            
            if (!cJSON_IsArray(m_element)) {
                // Replace current element with an array
                cJSON* newArray = cJSON_CreateArray();
                replace(newArray);
                m_element = newArray;
            }
            return ElementRef(m_element, index, m_read_only);
        }

        // Assignment operators
        ElementRef& operator=(int value) {
            if (m_read_only) return *this;
            
            cJSON* newItem = cJSON_CreateNumber(value);
            newItem->valueint = value;
            replace(newItem);
            m_element = newItem;
            return *this;
        }

        ElementRef& operator=(double value) {
            if (m_read_only) return *this;
            
            cJSON* newItem = cJSON_CreateNumber(value);
            replace(newItem);
            m_element = newItem;
            return *this;
        }

        ElementRef& operator=(const char* value) {
            if (m_read_only) return *this;
            
            cJSON* newItem = cJSON_CreateString(value);
            replace(newItem);
            m_element = newItem;
            return *this;
        }

        ElementRef& operator=(const std::string& value) {
            if (m_read_only) return *this;
            
            cJSON* newItem = cJSON_CreateString(value.c_str());
            replace(newItem);
            m_element = newItem;
            return *this;
        }

        ElementRef& operator=(bool value) {
            if (m_read_only) return *this;
            
            cJSON* newItem = value ? cJSON_CreateTrue() : cJSON_CreateFalse();
            replace(newItem);
            m_element = newItem;
            return *this;
        }

        ElementRef& operator=(const cJSONPP& value) {
            if (m_read_only) return *this;
            
            cJSON* newItem = cJSON_Duplicate(value.m_json, true);
            replace(newItem);
            m_element = newItem;
            return *this;
        }

        // Type conversion methods with default values
        int asInt(int defaultValue = 0) const {
            return isNumber() ? (int)m_element->valueint : defaultValue;
        }

        double asDouble(double defaultValue = NAN) const {
            return isNumber() ? m_element->valuedouble : defaultValue;
        }

        bool asBool(bool defaultValue = false) const {
            if (!m_element) return defaultValue;
            if (cJSON_IsTrue(m_element)) return true;
            if (cJSON_IsFalse(m_element)) return false;
            return defaultValue;
        }

        std::string asString(const std::string& defaultValue = "") const {
            if (!m_element) return defaultValue;
            
            if (cJSON_IsString(m_element)) {
                return m_element->valuestring ? m_element->valuestring : defaultValue;
            }
            
            if (cJSON_IsNull(m_element)) {
                return defaultValue;
            }
            
            // For other types, try to print as JSON
            char* str = cJSON_Print(m_element);
            if (!str) return defaultValue;
            
            std::string result = str;
            free(str);
            return result;
        }

        // String conversion operator
        operator std::string() const {
            return asString();
        }

        // Convert element to JSON string
        std::string toString(bool formatted = true) const {
            if (!m_element) return "null";
            
            char* str = formatted ? cJSON_Print(m_element) : cJSON_PrintUnformatted(m_element);
            if (!str) return "null";
            
            std::string result = str;
            free(str);
            return result;
        }

    private:
        // Replace the current element with a new one
        void replace(cJSON* newItem) {
            if (m_read_only) return;
            
            if (m_is_array) {
                cJSON_ReplaceItemInArray(m_parent, m_index, newItem);
            } else {
                cJSON_ReplaceItemInObject(m_parent, m_key.c_str(), newItem);
            }
        }
    };

    // Writable proxy for cJSONPP
    class WritableProxy {
    private:
        cJSONPP& m_parent;
    
    public:
        WritableProxy(cJSONPP& parent) : m_parent(parent) {}
        
        ElementRef operator[](const std::string& key) {
            return ElementRef(m_parent.m_json, key, false);
        }
        
        ElementRef operator[](int index) {
            if (!cJSON_IsArray(m_parent.m_json)) {
                if (m_parent.m_owned) {
                    cJSON_Delete(m_parent.m_json);
                }
                m_parent.m_json = cJSON_CreateArray();
                m_parent.m_owned = true;
            }
            return ElementRef(m_parent.m_json, index, false);
        }
        
        // Provide a read-only version of an ElementRef
        ElementRef readOnly(const std::string& key) const {
            return ElementRef(m_parent.m_json, key, true);
        }
        
        ElementRef readOnly(int index) const {
            return ElementRef(m_parent.m_json, index, true);
        }
        
        // Provide an explicitly writable version of an ElementRef
        ElementRef writableCopy(const std::string& key) {
            return ElementRef(m_parent.m_json, key, false);
        }
        
        ElementRef writableCopy(int index) {
            if (!cJSON_IsArray(m_parent.m_json)) {
                if (m_parent.m_owned) {
                    cJSON_Delete(m_parent.m_json);
                }
                m_parent.m_json = cJSON_CreateArray();
                m_parent.m_owned = true;
            }
            return ElementRef(m_parent.m_json, index, false);
        }
    };

public:
    WritableProxy w{*this};

    // Default constructor
    cJSONPP() : m_json(cJSON_CreateObject()), m_owned(true) {}

    // Constructor from existing cJSON object
    cJSONPP(cJSON* json, bool takeOwnership = false) : m_json(json), m_owned(takeOwnership) {
        if (!m_json) {
            m_json = cJSON_CreateObject();
            m_owned = true;
        }
    }

    // Copy constructor
    cJSONPP(const cJSONPP& other) : m_json(cJSON_Duplicate(other.m_json, true)), m_owned(true) {}

    // Move constructor
    cJSONPP(cJSONPP&& other) noexcept : m_json(other.m_json), m_owned(other.m_owned) {
        other.m_json = nullptr;
        other.m_owned = false;
    }

    // Destructor
    ~cJSONPP() {
        if (m_owned && m_json) {
            cJSON_Delete(m_json);
        }
    }

    // Assignment operators
    cJSONPP& operator=(const cJSONPP& other) {
        if (this != &other) {
            if (m_owned && m_json) {
                cJSON_Delete(m_json);
            }
            m_json = cJSON_Duplicate(other.m_json, true);
            m_owned = true;
        }
        return *this;
    }

    cJSONPP& operator=(cJSONPP&& other) noexcept {
        if (this != &other) {
            if (m_owned && m_json) {
                cJSON_Delete(m_json);
            }
            m_json = other.m_json;
            m_owned = other.m_owned;
            other.m_json = nullptr;
            other.m_owned = false;
        }
        return *this;
    }

    // Access elements - for reading only (doesn't modify JSON)
    ElementRef operator[](const std::string& key) const {
        return ElementRef(m_json, key, true);
    }

    ElementRef operator[](int index) const {
        return ElementRef(m_json, index, true);
    }

    // Get the underlying cJSON object
    cJSON* get() const {
        return m_json;
    }

    // Release ownership
    cJSON* release() {
        m_owned = false;
        return m_json;
    }

    // Create helpers
    static cJSONPP newArray() {
        return cJSONPP(cJSON_CreateArray(), true);
    }

    static cJSONPP newObject() {
        return cJSONPP(cJSON_CreateObject(), true);
    }

    char* to_c_str(bool formatted = true) const {
        char* str = formatted ? cJSON_Print(m_json) : cJSON_PrintUnformatted(m_json);
        return str;
    }
    
    // Convert to string
    std::string toString(bool formatted = true) const {
        char* str = to_c_str(formatted);
        std::string result = str ? str : "";
        if (str) free(str);
        return result;
    }
};

#endif // CJSONPP_H
