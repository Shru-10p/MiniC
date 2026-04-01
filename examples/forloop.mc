// Test for loop: sum 1..10, expected output: 55
func main() {
    var sum = 0;
    for (var i = 1; i <= 10; i = i + 1) {
        sum = sum + i;
    }
    print(sum);
}
