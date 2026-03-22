// arith.mc — arithmetic proof-of-concept
// Tests: literals, vars, +, -, *, /, %, operator precedence, print

func main() {
    var a = 6;
    var b = 7;
    var product = a * b;           // 42
    print(product);

    var sum = a + b * 2;           // 6 + 14 = 20  (precedence check)
    print(sum);

    var diff = product - sum;      // 42 - 20 = 22
    print(diff);

    var div = product / a;         // 42 / 6 = 7
    print(div);

    var mod = product % 10;        // 42 % 10 = 2
    print(mod);

    var neg = -a + b;              // -6 + 7 = 1
    print(neg);

    // Grouped expression
    var grouped = (a + b) * 2;     // 13 * 2 = 26
    print(grouped);

    return 0;
}
