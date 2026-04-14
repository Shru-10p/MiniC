// Test for loop and continue: sum odd numbers in 1..10, expected output: 25
func main() {
    var sum = 0;
    for (var i = 1; i <= 10; i = i + 1) {
        if(i % 2 == 0){
            i = i + 1;
            continue;
        }
        sum = sum + i;
    }
    print(sum);
}
