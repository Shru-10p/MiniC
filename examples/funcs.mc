// funcs.mc — function calls and recursion
// Tests: multi-param functions, recursion, mutual calls

func fib(n) {
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

func square(x) {
    return x * x;
}

func sumOfSquares(a, b) {
    return square(a) + square(b);
}

func main() {
    // Fibonacci sequence: 0 1 1 2 3 5 8 13
    var i = 0;
    while (i < 8) {
        print(fib(i));
        i = i + 1;
    }

    // Sum of squares: 3² + 4² = 25
    print(sumOfSquares(3, 4));

    return 0;
}
