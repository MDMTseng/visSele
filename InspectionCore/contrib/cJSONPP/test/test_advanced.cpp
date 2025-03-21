#include "../cJsonPP.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

// Helper macro for tests
#define ASSERT_TEST(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "Test failed: " << message << std::endl; \
            std::cerr << "Line " << __LINE__ << std::endl; \
            return 1; \
        } \
    } while (0)

int test_deep_nesting() {
    cJSONPP obj = cJSONPP::newObject();
    
    // Create a deeply nested structure
    obj.w["level1"]["level2"]["level3"]["level4"]["value"] = 42;
    
    // Test access at each level
    ASSERT_TEST(obj["level1"].isObject(), "Level 1 should be an object");
    ASSERT_TEST(obj["level1"]["level2"].isObject(), "Level 2 should be an object");
    ASSERT_TEST(obj["level1"]["level2"]["level3"].isObject(), "Level 3 should be an object");
    ASSERT_TEST(obj["level1"]["level2"]["level3"]["level4"].isObject(), "Level 4 should be an object");
    ASSERT_TEST(obj["level1"]["level2"]["level3"]["level4"]["value"].asInt() == 42, "Deep nested value incorrect");
    
    // Test non-existent deep path (should not throw, just return empty/default)
    ASSERT_TEST(obj["not"]["existing"]["path"].exists() == false, "Non-existent path should not exist");
    ASSERT_TEST(obj["not"]["existing"]["path"].asInt(-1) == -1, "Default value not returned for non-existent path");
    
    return 0;
}

int test_array_operations() {
    cJSONPP obj = cJSONPP::newObject();
    obj.w["array"] = cJSONPP::newArray();
    
    // Add different types to array
    obj.w["array"][0] = 100;
    obj.w["array"][1] = "string value";
    obj.w["array"][2] = true;
    obj.w["array"][3] = cJSONPP::newObject();
    obj.w["array"][3].w()["nested"] = "nested value";
    
    // Test array contents
    ASSERT_TEST(obj["array"][0].asInt() == 100, "Array int value incorrect");
    ASSERT_TEST(obj["array"][1].asString() == "string value", "Array string value incorrect");
    ASSERT_TEST(obj["array"][2].asBool() == true, "Array bool value incorrect");
    ASSERT_TEST(obj["array"][3].isObject(), "Array should contain an object at index 3");
    ASSERT_TEST(obj["array"][3]["nested"].asString() == "nested value", "Nested value in array incorrect");
    
    // Test array of arrays
    obj.w["matrix"] = cJSONPP::newArray();
    for (int i = 0; i < 3; i++) {
        obj.w["matrix"][i] = cJSONPP::newArray();
        for (int j = 0; j < 3; j++) {
            obj.w["matrix"][i][j] = i * 3 + j;
        }
    }
    
    // Verify matrix values
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            ASSERT_TEST(obj["matrix"][i][j].asInt() == i * 3 + j, 
                        "Matrix value incorrect at [" + std::to_string(i) + "][" + std::to_string(j) + "]");
        }
    }
    
    return 0;
}

int test_copy_ownership() {
    // Create original object
    cJSONPP original = cJSONPP::newObject();
    original.w["value"] = 42;
    
    // Test copy constructor
    cJSONPP copy(original);
    ASSERT_TEST(copy["value"].asInt() == 42, "Copy constructor didn't preserve values");
    
    // Modify copy and ensure original is unchanged
    copy.w["value"] = 100;
    ASSERT_TEST(copy["value"].asInt() == 100, "Modified value incorrect in copy");
    ASSERT_TEST(original["value"].asInt() == 42, "Original was modified when copy was changed");
    
    // Test assignment operator
    cJSONPP assigned = cJSONPP::newObject();
    assigned = original;
    ASSERT_TEST(assigned["value"].asInt() == 42, "Assignment operator didn't preserve values");
    
    // Modify assigned and ensure original is unchanged
    assigned.w["value"] = 200;
    ASSERT_TEST(assigned["value"].asInt() == 200, "Modified value incorrect in assigned object");
    ASSERT_TEST(original["value"].asInt() == 42, "Original was modified when assigned object was changed");
    
    return 0;
}

int main() {
    int result = 0;
    
    std::cout << "Running deep nesting test..." << std::endl;
    result = test_deep_nesting();
    if (result != 0) return result;
    
    std::cout << "Running array operations test..." << std::endl;
    result = test_array_operations();
    if (result != 0) return result;
    
    std::cout << "Running copy and ownership test..." << std::endl;
    result = test_copy_ownership();
    if (result != 0) return result;
    
    std::cout << "All advanced tests passed!" << std::endl;
    return 0;
} 