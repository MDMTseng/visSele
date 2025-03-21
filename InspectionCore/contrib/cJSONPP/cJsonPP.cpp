#include "cJsonPP.h"

// ElementRef implementations
cJSONPP::ElementRef::ElementRef(cJSON* parent, const std::string& key, bool readOnly)
    : m_parent(parent), m_is_array(false), m_index(-1), m_key(key), m_read_only(readOnly) 
{
    m_element = cJSON_GetObjectItem(parent, key.c_str());
    if (!m_element && !m_read_only) {
        m_element = cJSON_CreateNull();
        cJSON_AddItemToObject(parent, key.c_str(), m_element);
    }
}

cJSONPP::ElementRef::ElementRef(cJSON* parent, int index, bool readOnly)
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

cJSONPP::ElementRef::ElementRef(const ElementRef& other, bool forceReadOnly)
    : m_parent(other.m_parent), m_element(other.m_element), m_is_array(other.m_is_array),
      m_index(other.m_index), m_key(other.m_key), m_read_only(forceReadOnly || other.m_read_only)
{
    // This simply copies all fields but forces read-only if requested
}

cJSONPP::ElementRef::ElementRef(const ElementRef& other)
    : m_parent(other.m_parent), m_element(other.m_element), m_is_array(other.m_is_array),
      m_index(other.m_index), m_key(other.m_key), m_read_only(true)
{
    // Default copy creates a read-only version
}

cJSONPP::ElementRef& cJSONPP::ElementRef::operator=(const ElementRef& other)
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

cJSON* cJSONPP::ElementRef::get() const 
{ 
    return m_element; 
}

bool cJSONPP::ElementRef::exists() const 
{ 
    return m_element != nullptr; 
}

bool cJSONPP::ElementRef::isNumber() const 
{ 
    return m_element != nullptr && cJSON_IsNumber(m_element); 
}

bool cJSONPP::ElementRef::isString() const 
{ 
    return m_element != nullptr && cJSON_IsString(m_element); 
}

bool cJSONPP::ElementRef::isBool() const 
{ 
    return m_element != nullptr && (cJSON_IsTrue(m_element) || cJSON_IsFalse(m_element)); 
}

bool cJSONPP::ElementRef::isObject() const 
{ 
    return m_element != nullptr && cJSON_IsObject(m_element); 
}

bool cJSONPP::ElementRef::isArray() const 
{ 
    return m_element != nullptr && cJSON_IsArray(m_element); 
}

bool cJSONPP::ElementRef::isNull() const 
{ 
    return m_element != nullptr && cJSON_IsNull(m_element); 
}

int cJSONPP::ElementRef::length() const
{
    if (m_element != nullptr && cJSON_IsArray(m_element)) {
        return cJSON_GetArraySize(m_element);
    }
    return -1;
}

cJSONPP::ElementRef cJSONPP::ElementRef::w() 
{
    if (!m_element) return ElementRef(nullptr, m_key, false);
    if (m_is_array) {
        return ElementRef(m_parent, m_index, false);
    } else {
        return ElementRef(m_parent, m_key, false);
    }
}

cJSONPP::ElementRef cJSONPP::ElementRef::writableCopy() const 
{
    return ElementRef(*this, false); // Use the special constructor to create a writable copy
}

cJSONPP::ElementRef cJSONPP::ElementRef::readOnly() const 
{
    return ElementRef(*this, true); // Use the special constructor to create a read-only copy
}

cJSONPP::ElementRef cJSONPP::ElementRef::safeAccess(const std::string& key) const 
{
    if (!exists() || !cJSON_IsObject(m_element)) {
        return ElementRef(nullptr, key, true);
    }
    return ElementRef(m_element, key, true);
}

cJSONPP::ElementRef cJSONPP::ElementRef::safeAccess(int index) const 
{
    if (!exists() || !cJSON_IsArray(m_element) || index >= cJSON_GetArraySize(m_element)) {
        return ElementRef(nullptr, index, true);
    }
    return ElementRef(m_element, index, true);
}

cJSONPP::ElementRef cJSONPP::ElementRef::operator[](const std::string& key) 
{
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

cJSONPP::ElementRef cJSONPP::ElementRef::operator[](int index) 
{
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

cJSONPP::ElementRef& cJSONPP::ElementRef::operator=(int value) 
{
    if (m_read_only) return *this;
    
    cJSON* newItem = cJSON_CreateNumber(value);
    newItem->valueint = value;
    replace(newItem);
    m_element = newItem;
    return *this;
}

cJSONPP::ElementRef& cJSONPP::ElementRef::operator=(double value) 
{
    if (m_read_only) return *this;
    
    cJSON* newItem = cJSON_CreateNumber(value);
    replace(newItem);
    m_element = newItem;
    return *this;
}

cJSONPP::ElementRef& cJSONPP::ElementRef::operator=(const char* value) 
{
    if (m_read_only) return *this;
    
    cJSON* newItem = cJSON_CreateString(value);
    replace(newItem);
    m_element = newItem;
    return *this;
}

cJSONPP::ElementRef& cJSONPP::ElementRef::operator=(const std::string& value) 
{
    if (m_read_only) return *this;
    
    cJSON* newItem = cJSON_CreateString(value.c_str());
    replace(newItem);
    m_element = newItem;
    return *this;
}

cJSONPP::ElementRef& cJSONPP::ElementRef::operator=(bool value) 
{
    if (m_read_only) return *this;
    
    cJSON* newItem = value ? cJSON_CreateTrue() : cJSON_CreateFalse();
    replace(newItem);
    m_element = newItem;
    return *this;
}

cJSONPP::ElementRef& cJSONPP::ElementRef::operator=(const cJSONPP& value) 
{
    if (m_read_only) return *this;
    
    cJSON* newItem = cJSON_Duplicate(value.m_json, true);
    replace(newItem);
    m_element = newItem;
    return *this;
}

int cJSONPP::ElementRef::asInt(int defaultValue) const 
{
    return isNumber() ? (int)m_element->valueint : defaultValue;
}

double cJSONPP::ElementRef::asDouble(double defaultValue) const 
{
    return isNumber() ? m_element->valuedouble : defaultValue;
}

bool cJSONPP::ElementRef::asBool(bool defaultValue) const 
{
    if (!m_element) return defaultValue;
    if (cJSON_IsTrue(m_element)) return true;
    if (cJSON_IsFalse(m_element)) return false;
    return defaultValue;
}

std::string cJSONPP::ElementRef::asString(const std::string& defaultValue) const 
{
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

cJSONPP::ElementRef::operator std::string() const 
{
    return asString();
}

std::string cJSONPP::ElementRef::toString(bool formatted) const 
{
    if (!m_element) return "null";
    
    char* str = formatted ? cJSON_Print(m_element) : cJSON_PrintUnformatted(m_element);
    if (!str) return "null";
    
    std::string result = str;
    free(str);
    return result;
}

void cJSONPP::ElementRef::replace(cJSON* newItem) 
{
    if (m_read_only) return;
    
    if (m_is_array) {
        cJSON_ReplaceItemInArray(m_parent, m_index, newItem);
    } else {
        cJSON_ReplaceItemInObject(m_parent, m_key.c_str(), newItem);
    }
}

// ElementRef::DeleteProxy implementations
cJSONPP::ElementRef::DeleteProxy::DeleteProxy(ElementRef& ref) : m_ref(ref) {}

bool cJSONPP::ElementRef::DeleteProxy::operator[](const std::string& key)
{
    return m_ref.deleteItem(key);
}

bool cJSONPP::ElementRef::DeleteProxy::operator[](int index)
{
    return m_ref.deleteItem(index);
}

// ElementRef deleteItem implementations
bool cJSONPP::ElementRef::deleteItem(const std::string& key)
{
    if (m_read_only || !m_element || !cJSON_IsObject(m_element)) {
        return false;
    }
    
    cJSON_DeleteItemFromObject(m_element, key.c_str());
    return true;
}

bool cJSONPP::ElementRef::deleteItem(int index)
{
    if (m_read_only || !m_element || !cJSON_IsArray(m_element) || 
        index < 0 || index >= cJSON_GetArraySize(m_element)) {
        return false;
    }
    
    cJSON_DeleteItemFromArray(m_element, index);
    return true;
}

// WritableProxy implementations
cJSONPP::WritableProxy::WritableProxy(cJSONPP& parent) : m_parent(parent) {}

cJSONPP::ElementRef cJSONPP::WritableProxy::operator[](const std::string& key) 
{
    return ElementRef(m_parent.m_json, key, false);
}

cJSONPP::ElementRef cJSONPP::WritableProxy::operator[](int index) 
{
    if (!cJSON_IsArray(m_parent.m_json)) {
        if (m_parent.m_owned) {
            cJSON_Delete(m_parent.m_json);
        }
        m_parent.m_json = cJSON_CreateArray();
        m_parent.m_owned = true;
    }
    return ElementRef(m_parent.m_json, index, false);
}

cJSONPP::ElementRef cJSONPP::WritableProxy::readOnly(const std::string& key) const 
{
    return ElementRef(m_parent.m_json, key, true);
}

cJSONPP::ElementRef cJSONPP::WritableProxy::readOnly(int index) const 
{
    return ElementRef(m_parent.m_json, index, true);
}

cJSONPP::ElementRef cJSONPP::WritableProxy::writableCopy(const std::string& key) 
{
    return ElementRef(m_parent.m_json, key, false);
}

cJSONPP::ElementRef cJSONPP::WritableProxy::writableCopy(int index) 
{
    if (!cJSON_IsArray(m_parent.m_json)) {
        if (m_parent.m_owned) {
            cJSON_Delete(m_parent.m_json);
        }
        m_parent.m_json = cJSON_CreateArray();
        m_parent.m_owned = true;
    }
    return ElementRef(m_parent.m_json, index, false);
}

// DeleteProxy implementations
cJSONPP::DeleteProxy::DeleteProxy(cJSONPP& parent) : m_parent(parent) {}

bool cJSONPP::DeleteProxy::operator[](const std::string& key)
{
    return m_parent.deleteItem(key);
}

bool cJSONPP::DeleteProxy::operator[](int index)
{
    return m_parent.deleteItem(index);
}

// cJSONPP implementations
cJSONPP::cJSONPP() : m_json(cJSON_CreateObject()), m_owned(true) {}

cJSONPP::cJSONPP(cJSON* json, bool takeOwnership) : m_json(json), m_owned(takeOwnership) 
{
    if (!m_json) {
        m_json = cJSON_CreateObject();
        m_owned = true;
    }
}

cJSONPP::cJSONPP(const cJSONPP& other) : m_json(cJSON_Duplicate(other.m_json, true)), m_owned(true) {}

cJSONPP::cJSONPP(cJSONPP&& other) noexcept : m_json(other.m_json), m_owned(other.m_owned) 
{
    other.m_json = nullptr;
    other.m_owned = false;
}

cJSONPP::~cJSONPP() 
{
    if (m_owned && m_json) {
        cJSON_Delete(m_json);
    }
}

cJSONPP& cJSONPP::operator=(const cJSONPP& other) 
{
    if (this != &other) {
        if (m_owned && m_json) {
            cJSON_Delete(m_json);
        }
        m_json = cJSON_Duplicate(other.m_json, true);
        m_owned = true;
    }
    return *this;
}

cJSONPP& cJSONPP::operator=(cJSONPP&& other) noexcept 
{
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

cJSONPP::ElementRef cJSONPP::operator[](const std::string& key) const 
{
    return ElementRef(m_json, key, true);
}

cJSONPP::ElementRef cJSONPP::operator[](int index) const 
{
    return ElementRef(m_json, index, true);
}

cJSON* cJSONPP::get() const 
{
    return m_json;
}

cJSON* cJSONPP::release() 
{
    m_owned = false;
    return m_json;
}

cJSONPP cJSONPP::newArray() 
{
    return cJSONPP(cJSON_CreateArray(), true);
}

cJSONPP cJSONPP::newObject() 
{
    return cJSONPP(cJSON_CreateObject(), true);
}

char* cJSONPP::to_c_str(bool formatted) const 
{
    char* str = formatted ? cJSON_Print(m_json) : cJSON_PrintUnformatted(m_json);
    return str;
}

std::string cJSONPP::toString(bool formatted) const 
{
    char* str = to_c_str(formatted);
    std::string result = str ? str : "";
    if (str) free(str);
    return result;
}

int cJSONPP::length() const
{
    if (m_json && cJSON_IsArray(m_json)) {
        return cJSON_GetArraySize(m_json);
    }
    return -1;
}

bool cJSONPP::deleteItem(const std::string& key)
{
    if (!m_json || !cJSON_IsObject(m_json)) {
        return false;
    }
    
    cJSON_DeleteItemFromObject(m_json, key.c_str());
    return true;
}

bool cJSONPP::deleteItem(int index)
{
    if (!m_json || !cJSON_IsArray(m_json) || index < 0 || index >= cJSON_GetArraySize(m_json)) {
        return false;
    }
    
    cJSON_DeleteItemFromArray(m_json, index);
    return true;
} 