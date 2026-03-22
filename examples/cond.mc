// cond.mc — conditionals and loops proof-of-concept
// Tests: if/else, while, comparisons, logical ops

func abs(x) {
    if (x < 0) {
        return -x;
    }
    return x;
}

func max(a, b) {
    if (a >= b) {
        return a;
    }
    return b;
}

func main() {
    // Basic if/else
    var x = 10;
    if (x > 5) {
        print(1);    // expected: 1
    } else {
        print(0);
    }

    // Nested conditions
    var y = -3;
    var a = abs(y);
    print(a);        // expected: 3

    // max
    print(max(4, 9));   // expected: 9
    print(max(7, 2));   // expected: 7

    // Logical AND / OR
    var p = 1;
    var q = 0;
    print(p && q);   // expected: 0
    print(p || q);   // expected: 1
    print(!p);       // expected: 0

    // while loop: sum 1..5
    var i = 1;
    var sum = 0;
    while (i <= 5) {
        sum = sum + i;
        i = i + 1;
    }
    print(sum);      // expected: 15

    // Countdown
    var n = 3;
    while (n > 0) {
        print(n);
        n = n - 1;
    }
    // expected: 3, 2, 1

    return 0;
}
