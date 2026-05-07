let arr = [];
for (let i = 0; i < 5000; i++) arr.push(5000 - i);
let start = Date.now();
let n = arr.length;
for (let i = 0; i < n; i++) {
    for (let j = 0; j < n - i - 1; j++) {
        if (arr[j] > arr[j + 1]) {
            let temp = arr[j];
            arr[j] = arr[j + 1];
            arr[j + 1] = temp;
        }
    }
}
let end = Date.now();
console.log(`JavaScript: ${end - start} ms (First: ${arr[0]}, Last: ${arr[4999]})`);
