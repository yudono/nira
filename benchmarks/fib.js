function fib(n) {
    if (n < 2) return n;
    return fib(n-1) + fib(n-2);
}

const start = Date.now();
const result = fib(35);
const end = Date.now();

console.log(`JavaScript (Node.js): ${end - start} ms (Result: ${result})`);
