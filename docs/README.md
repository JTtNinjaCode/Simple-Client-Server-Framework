# Simple Client-Server Framework

This is a simple client-server framework implemented in C++ that uses TCP connections. It creates a Connection class to wrap the socket and defines a simple custom Message format as the basic unit of transmission.

The implementation is quite simple. If you have experience with Boost.asio, you can directly read the source code and review the examples of both client and server implementations.

### Transmission Format

A Message is divided into a Header and a Body. The Header includes an Operation and a Size. The Operation is a user-defined enum, and the Size represents the size of the Body in bytes. The Body contains all the data. Data is pushed into the body using the << operator and popped out using the >> operator, functioning similarly to a stack.

Usage example:

```cpp
// Push data with << and retrieve data with >>
int num1 = 100, num2 = 0;
msg << num1;
msg >> num2;
std::cout << num2 << std::endl;

// Function overloads support std::string
std::string str1 = "Hello, world";
std::string str2;
msg << str1;
msg >> str2;
std::cout << str2 << std::endl;
```

### Dependency

- Boost Asio

### How to build

Verify that Boost is installed and detectable by CMake, and then run `build_windows.bat`.