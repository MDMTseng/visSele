
#include <string>
#include <vector>
class CompScript
{
private:
    void* scriptImplObj;
public:
    std::vector<std::string> failInfo;
    CompScript();
    void set_variable(std::string name, float &value);
    void add_variable(std::string name, float &value);

    void add_function(std::string name, float &value);

    bool compile(std::string script);

    float value();

    ~CompScript();

};