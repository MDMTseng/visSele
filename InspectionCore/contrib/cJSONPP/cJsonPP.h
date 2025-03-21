#ifndef CJSONPP_H
#define CJSONPP_H

#include <string>
#include <iostream>
#include "../cJSON/cJSON.h"

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
        ElementRef(cJSON* parent, const std::string& key, bool readOnly = true);

        // Constructor for array element
        ElementRef(cJSON* parent, int index, bool readOnly = true);
        
        // Special constructor for creating a read-only copy
        ElementRef(const ElementRef& other, bool forceReadOnly);
        
        // Copy constructor - creates a read-only copy by default
        ElementRef(const ElementRef& other);
        
        // Copy assignment operator - creates a read-only copy by default
        ElementRef& operator=(const ElementRef& other);

        // Get the underlying cJSON element
        cJSON* get() const;

        // Check if element exists and has expected type
        bool exists() const;
        bool isNumber() const;
        bool isString() const;
        bool isBool() const;
        bool isObject() const;
        bool isArray() const;
        bool isNull() const;

        // Write-enabled version of this element
        ElementRef w();

        // Create a writable copy
        ElementRef writableCopy() const;

        // Read-only version of this element
        ElementRef readOnly() const;

        // Safe access methods (read-only, won't modify structure)
        ElementRef safeAccess(const std::string& key) const;
        ElementRef safeAccess(int index) const;

        // Nested property access
        ElementRef operator[](const std::string& key);

        // Nested array access
        ElementRef operator[](int index);

        // Assignment operators
        ElementRef& operator=(int value);
        ElementRef& operator=(double value);
        ElementRef& operator=(const char* value);
        ElementRef& operator=(const std::string& value);
        ElementRef& operator=(bool value);
        ElementRef& operator=(const cJSONPP& value);

        // Type conversion methods with default values
        int asInt(int defaultValue = 0) const;
        double asDouble(double defaultValue = NAN) const;
        bool asBool(bool defaultValue = false) const;
        std::string asString(const std::string& defaultValue = "") const;

        // String conversion operator
        operator std::string() const;

        // Convert element to JSON string
        std::string toString(bool formatted = true) const;

    private:
        // Replace the current element with a new one
        void replace(cJSON* newItem);
    };

    // Writable proxy for cJSONPP
    class WritableProxy {
    private:
        cJSONPP& m_parent;
    
    public:
        WritableProxy(cJSONPP& parent);
        
        ElementRef operator[](const std::string& key);
        ElementRef operator[](int index);
        
        // Provide a read-only version of an ElementRef
        ElementRef readOnly(const std::string& key) const;
        ElementRef readOnly(int index) const;
        
        // Provide an explicitly writable version of an ElementRef
        ElementRef writableCopy(const std::string& key);
        ElementRef writableCopy(int index);
    };

public:
    WritableProxy w{*this};

    // Default constructor
    cJSONPP();

    // Constructor from existing cJSON object
    cJSONPP(cJSON* json, bool takeOwnership = false);

    // Copy constructor
    cJSONPP(const cJSONPP& other);

    // Move constructor
    cJSONPP(cJSONPP&& other) noexcept;

    // Destructor
    ~cJSONPP();

    // Assignment operators
    cJSONPP& operator=(const cJSONPP& other);
    cJSONPP& operator=(cJSONPP&& other) noexcept;

    // Access elements - for reading only (doesn't modify JSON)
    ElementRef operator[](const std::string& key) const;
    ElementRef operator[](int index) const;

    // Get the underlying cJSON object
    cJSON* get() const;

    // Release ownership
    cJSON* release();

    // Create helpers
    static cJSONPP newArray();
    static cJSONPP newObject();

    char* to_c_str(bool formatted = true) const;
    
    // Convert to string
    std::string toString(bool formatted = true) const;
};

#endif // CJSONPP_H



/*
example:

cJSON *jobj = cJSON_CreateObject();
cJSONPP nobj(jobj, true);

// Create a nested path with writes
nobj.w["x"]["y"]["z"] = 123;


*/