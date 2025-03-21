#include "../cJSONPP.h"
#include <cassert>
#include <iostream>
#include <string>

// Helper macro for tests
#define ASSERT_TEST(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "Test failed: " << message << std::endl; \
            std::cerr << "Line " << __LINE__ << std::endl; \
            return 1; \
        } \
    } while (0)

int test_basic_operations() {
    cJSONPP obj = cJSONPP::newObject();
    
    // Test setting and getting basic types
    obj.w()["number"] = 42;
    obj.w()["string"] = "test string";
    obj.w()["boolean"] = true;
    
    ASSERT_TEST(obj["number"].asInt() == 42, "Integer value mismatch");
    ASSERT_TEST(obj["string"].asString() == "test string", "String value mismatch");
    ASSERT_TEST(obj["boolean"].asBool() == true, "Boolean value mismatch");
    
    // Test default values for non-existent properties
    ASSERT_TEST(obj["non_existent"].asInt(10) == 10, "Default integer value failed");
    ASSERT_TEST(obj["non_existent"].asString("default") == "default", "Default string value failed");
    
    // Test nested objects
    obj.w()["nested"]["a"] = 1;
    obj.w()["nested"]["b"] = 2;
    
    ASSERT_TEST(obj["nested"]["a"].asInt() == 1, "Nested property access failed");
    ASSERT_TEST(obj["nested"]["b"].asInt() == 2, "Nested property access failed");
    
    // Test array creation and access
    obj.w()["array"] = cJSONPP::newArray();
    obj.w()["array"][0] = 10;
    obj.w()["array"][1] = 20;
    obj.w()["array"][2] = 30;
    
    ASSERT_TEST(obj["array"][0].asInt() == 10, "Array access failed");
    ASSERT_TEST(obj["array"][1].asInt() == 20, "Array access failed");
    ASSERT_TEST(obj["array"][2].asInt() == 30, "Array access failed");
    
    return 0;
}

int test_type_checking() {
    cJSONPP obj = cJSONPP::newObject();
    
    obj.w()["number"] = 42;
    obj.w()["string"] = "test";
    obj.w()["boolean"] = true;
    obj.w()["object"] = cJSONPP::newObject();
    obj.w()["array"] = cJSONPP::newArray();
    
    ASSERT_TEST(obj["number"].isNumber(), "Number type check failed");
    ASSERT_TEST(obj["string"].isString(), "String type check failed");
    ASSERT_TEST(obj["boolean"].isBool(), "Boolean type check failed");
    ASSERT_TEST(obj["object"].isObject(), "Object type check failed");
    ASSERT_TEST(obj["array"].isArray(), "Array type check failed");
    
    // Cross-checks (should be false)
    ASSERT_TEST(!obj["number"].isString(), "Number shouldn't be a string");
    ASSERT_TEST(!obj["string"].isNumber(), "String shouldn't be a number");
    
    return 0;
}

int test_string_conversion() {
    cJSONPP obj = cJSONPP::newObject();
    
    obj.w()["key"] = "value";
    obj.w()["nested"]["a"] = 1;
    obj.w()["array"][0] = 10;
    if(obj.w()["array"].isArray()){
        //check array length
        std::cout << "array length: " << obj.w()["array"].length() << std::endl;
        std::cout << "array length: " << obj.length() << std::endl;
    }

    
    {
        cJSONPP arr = cJSONPP::newArray();
        arr.w()[0] = 10;
        arr.w()[1] = 20;
        arr.w()[2] = 30;
        std::cout << "array length: " << arr.length() << std::endl;
        //print array
        std::cout << "array: " << arr.toString() << std::endl;
    }
    
    std::string json = obj.toString(false); // unformatted
    
    // Basic check to ensure JSON string contains expected elements
    ASSERT_TEST(json.find("\"key\":\"value\"") != std::string::npos, "JSON string missing key-value pair");
    ASSERT_TEST(json.find("\"nested\":{\"a\":1}") != std::string::npos, "JSON string missing nested object");
    ASSERT_TEST(json.find("\"array\":[10]") != std::string::npos, "JSON string missing array");
    
    return 0;
}

int test_reassignment() {
    cJSONPP obj = cJSONPP::newObject();
    obj.w()["number"] = cJSONPP::newObject();
    std::cout << "obj.number={} " << obj.toString() << std::endl;
    obj.w()["number"] = 100;
    std::cout << "obj.number=100 " << obj.toString() << std::endl;
    ASSERT_TEST(obj["number"].isNumber(), "number should be a number");

    obj.del()["number"];//delete the number
    std::cout << "delete number from obj: " << obj.toString() << std::endl;

    obj.w()["a"]["b"]["c"]=10;
    std::cout << "obj.a.b.c=10: obj=" << obj.toString() << std::endl;
    auto b=obj["a"]["b"];
    b.w().del["c"];
    std::cout << "delete obj.a.b.c: obj=" << obj.toString() << std::endl;


    return 0;
}
int main() {
    int result = 0;
    
    std::cout << "Running basic operations test..." << std::endl;
    result = test_basic_operations();
    if (result != 0) return result;
    
    std::cout << "Running type checking test..." << std::endl;
    result = test_type_checking();
    if (result != 0) return result;
    
    std::cout << "Running string conversion test..." << std::endl;
    result = test_string_conversion();
    if (result != 0) return result;

    std::cout << "Running reassignment & deletion test..." << std::endl;
    result = test_reassignment();
    if (result != 0) return result;


    
    std::cout << "All tests passed!" << std::endl;
    return 0;
} 