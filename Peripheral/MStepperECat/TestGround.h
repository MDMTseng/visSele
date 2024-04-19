#include <iostream>
#include <stack>
#include <cctype>
#include <sstream>

// Function to return precedence of operators
int precedence(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    if (op == '^') return 3;
    return 0;
}

// Check if the character is an operator
bool isOperator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '^';
}

// Helper struct to store functions and their parameter count
struct Function {
    std::string name;
    int paramCount;
};

// Function to convert infix expression to postfix
std::string infixToPostfix(const std::string &infix) {
    std::stack<char> operators;
    std::stack<Function> functions;
    std::string postfix = "";
    int dcount=0;
    for (int i = 0; i < infix.length(); i++) {
        if (infix[i] == ' ') continue;
        dcount++;
        if (isdigit(infix[i]) || infix[i]=='.') { // Handle numbers
            while (i < infix.length() && (isdigit(infix[i])|| infix[i]=='.')) {
                postfix += infix[i];
                i++;
            }
            i--; // Adjust for loop increment
            postfix += ' ';
        }
        else if (isalpha(infix[i])) { // Handle identifiers (function names)
            std::string func = "";
            while (i < infix.length() && isalpha(infix[i])) {
                func += infix[i];
                i++;
            }
            i--; // Adjust for loop increment
            functions.push({func, 0}); // Push function name onto std::stack
            operators.push('('); // Assume a '(' follows function name
        }
        else if (infix[i] == '(') {
            // Already handled with functions
            dcount=0;
        }
        else if (infix[i] == ')') {
            while (!operators.empty() && operators.top() != '(') {
                postfix += operators.top();
                postfix += ' ';
                operators.pop();
            }
            operators.pop(); // Remove '('
            if (!functions.empty()) {
                Function func = functions.top();
                functions.pop();
                postfix += func.name + "@" + std::to_string(func.paramCount+((dcount>1)?1:0)) + " "; // Append function name after arguments
            }
        }
        else if (isOperator(infix[i])) {
            while (!operators.empty() && precedence(operators.top()) >= precedence(infix[i])) {
                postfix += operators.top();
                postfix += ' ';
                operators.pop();
            }
            operators.push(infix[i]);
        }
        else if (infix[i] == ',') {
            while (!operators.empty() && operators.top() != '(') {
                postfix += operators.top();
                postfix += ' ';
                operators.pop();
            }
            // postfix += ',';
//            postfix += ' '; // Space after comma
            if (!functions.empty()) {
                functions.top().paramCount++; // Increment parameter count for the current function
            }
        }
    }

    // Pop all the operators left in the std::stack
    while (!operators.empty()) {
        postfix += operators.top();
        postfix += ' ';
        operators.pop();
    }

    return postfix;
}



/*
#include <iostream>
#include <cmath>

typedef double (*op_func)(double, double);

typedef union {
    double num;
    op_func op;
} Data;

typedef struct {
    int isOp; // 0 for number, 1 for operation
    Data data;
} StackElement;

// Stack operations
void push(StackElement stack[], int *top, StackElement elem) {
    stack[++(*top)] = elem;
}

StackElement pop(StackElement stack[], int *top) {
    return stack[(*top)--];
}

// Arithmetic operations
double add(double a, double b) { return a + b; }
double multiply(double a, double b) { return a * b; }
double power(double a, double b) { return std::pow(a, b); }

// Evaluate postfix expression using a stack
void evaluatePostfix(StackElement expr[], int length) {
    StackElement stack[100];
    int top = -1;
    
    for (int i = 0; i < length; i++) {
        if (expr[i].isOp) {
            StackElement right = pop(stack, &top);
            StackElement left = pop(stack, &top);
            double result = expr[i].data.op(left.data.num, right.data.num);
            push(stack, &top, {0, {.num = result}});
        } else {
            push(stack, &top, expr[i]);
        }
    }

    if (top == 0) {
        std::cout << "The result is: " << stack[top].data.num << std::endl;
    } else {
        std::cerr << "Error in evaluation." << std::endl;
    }
}

int main() {
    // Example postfix expression: "3 4 + 5 6 * +"
    StackElement expr[] = {
        {0, {.num = 3}},
        {0, {.num = 4}},
        {1, {.op = add}},
        {0, {.num = 5}},
        {0, {.num = 6}},
        {1, {.op = multiply}},
        {1, {.op = add}}
    };
    int length = sizeof(expr) / sizeof(expr[0]);

    evaluatePostfix(expr, length);
    return 0;
}
*/
