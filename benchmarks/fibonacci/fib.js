function fib(n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

let start = Date.now();
let result = fib(40);
let end = Date.now();
console.log(`JavaScript: ${end - start} ms (Result: ${result})`);
