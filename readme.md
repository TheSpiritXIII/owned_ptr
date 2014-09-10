owned_ptr
=========
owned_ptr is a small class created in C++11 designed as a smart pointer for
external libraries (e.g. plugins). It was created to solve the similar issue of:

    #include <iostream>
    
    int increment(int *value)
    {
        return (*value)++;
    }
    
    int main()
    {
        int *some_int = new int(3);
        std::thread thread(&increment, int));
        
        // The result of this is undefined.
        // If the thread executes first, then no issues.
        // Otherwise, the application crashes.
        delete int;
        
        thread.join();
        return 0;
        
    }

External libraries usually contain their own heap segments. Thus, there is no
telling when a variable is invalidated and becomes a dangling pointer. If you
wish to allow users to disable/remove external libraries at runtime, then the
issue above is apparent. owned_ptr solves this issue.

The owned_ptr class relies on another class child_ptr which stores a reference
to the owned_ptr class and vice-versa. child_ptr is completely thread safe and
free from any memory errors.

Usage
-----
To use, simply include the owned_ptr.hpp file.

For usage example, see example.cpp.

Disclaimer
----------
The owned_ptr class uses C++11 STL classes internally. Unless you replace the
STL classes yourself, you cannot inter-use these classes throughout different
compiler runtimes (so you must use the same compiler for the application and all
plugins). Hopefully, you already knew this and should know why.