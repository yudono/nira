let start = Date.now();
let sum = 0;
for (let i = 0; i < 100000000; i++) {
    sum += i;
}
let end = Date.now();
console.log(`JavaScript: ${end - start} ms (Result: ${sum})`);
