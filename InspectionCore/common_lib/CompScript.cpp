#include "CompScript.hpp"
#include "cJSON.h"
#include <cstdio>
#include <string>
#include <map>

#include "exprtk.hpp"

class scriptImpl
{
    private:

   typedef exprtk::symbol_table<float> symbol_table_t;
   typedef exprtk::expression<float>   expression_t;
   typedef exprtk::parser<float>       parser_t;



    expression_t expression;
    symbol_table_t symbol_table;
    parser_t parser;

    std::map<std::string,float> varTable;

public:
    scriptImpl()
    {
    }

template <typename T>
    void set_variable(std::string name, T &value)
    {
        varTable[name]=value;
    }
    
template <typename T>
    void add_variable(std::string name, T &value)
    {
        varTable[name]=value;
        float &v=varTable[name];
        symbol_table.add_variable(name,v);
    }

template <typename T>
    void add_function(std::string name, T &value)
    {
        symbol_table.add_function(name,value);
    }

    bool compile(std::string script)
    {
        expression.register_symbol_table(symbol_table);

        return parser.compile(script,expression);
    }


    int get_compileFailInfo(std::vector<std::string> &failInfo)
    {
        failInfo.clear();
        for (std::size_t i = 0; i < parser.error_count(); ++i) {
            typedef exprtk::parser_error::type error_t;
            error_t error = parser.get_error(i);

            std::string errorInfo="Error[" + std::to_string(i) + "]: " + error.diagnostic + " at Position: " + std::to_string(error.token.position); 
            failInfo.push_back(errorInfo);
        }
        return parser.error_count();
    }



    float value()
    {
        return expression.value();
    }


    ~scriptImpl()
    {
    }
};


CompScript::CompScript()
{   
    this->scriptImplObj=(void*)new scriptImpl();
}




void CompScript::set_variable(std::string name, float &value)
{
    scriptImpl *impl=(scriptImpl*)this->scriptImplObj;
    impl->set_variable<float>(name,value);
}

void CompScript::add_variable(std::string name, float &value)
{
    scriptImpl *impl=(scriptImpl*)this->scriptImplObj;
    impl->add_variable<float>(name,value);
}

//make a exprtk functioin warper
void CompScript::add_function(std::string name, float &value)
{
    // scriptImpl *impl=(scriptImpl*)this->scriptImplObj;
    // impl->add_function(name,value);
}

bool CompScript::compile(std::string script)
{
    scriptImpl *impl=(scriptImpl*)this->scriptImplObj;
    failInfo.clear();
    bool success=impl->compile(script);
    if(success==false)
    {
        impl->get_compileFailInfo(failInfo);
    }


    return success;
}



float CompScript::value()
{
    scriptImpl *impl=(scriptImpl*)this->scriptImplObj;
    return impl->value();
}


CompScript::~CompScript()
{
    scriptImpl *impl=(scriptImpl*)this->scriptImplObj;
    delete impl;
    this->scriptImplObj=NULL;
}



// template <typename T>
// void polynomial()
// {
//    typedef exprtk::symbol_table<T> symbol_table_t;
//    typedef exprtk::expression<T>   expression_t;
//    typedef exprtk::parser<T>       parser_t;

//    const std::string expression_string =
//       "25x^5 - 35x^4 - 15x^3 + 40x^2 - 15x + 1";

//    const T r0 = T(0);
//    const T r1 = T(1);
//          T  x = T(0);

//    symbol_table_t symbol_table;
//    symbol_table.add_variable("x",x);

//    expression_t expression;
//    expression.register_symbol_table(symbol_table);

//    parser_t parser;
//    parser.compile(expression_string,expression);

//    const T delta = T(1.0 / 100.0);

//    for (x = r0; x <= r1; x += delta)
//    {
//       printf("%19.15f\t%19.15f\n", x, expression.value());
//    }
// }

// int main()
// {
//    polynomial<double>();
//    return 0;
// }
