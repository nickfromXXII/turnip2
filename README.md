# turnip2

Welcome to turnip2! turnip2 is cross-platform open-source compiler for Turnip2 programming language. Turnip2 is the second generation of Turnip programming language.
There is a simple [interpreter](https://github.com/NEzyaka/Turnip-Runner) and an ~~IDE~~ [code editor](https://github.com/NEzyaka/Turnip-Editor) for Turnip.

Turnip2 is a C-like strongly typed programming language for beginners in programming.
Here is the sample code (other examples you can find in `examples/` dir):
```
function main() {
    var a: int = 123;
    var b: int;

    if not a < 0 or a is 255 {
        println "a is more than 0 or a is 255";
    }

    println 2+2*2; // will print 6
}
```

## Building from source
To build turnip2 you need LLVM C API. ([how to install it](http://llvm.org/docs/GettingStarted.html))

Make sure that cmake version 3.6 or newer is installed!

1. Get the turnip2 source code: `git clone https://github.com/nezyaka/turnip2`;
2. Go to the turnip2 source code directory: `cd turnip2`;
3. Create a build directory: `mkdir build && cd build`;
4. Execute this: `cmake ..`;
5. Build it: `cmake --build .`;
6. Now you can use turnip2.

