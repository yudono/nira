console.time("JS Bubble Sort");
let arr = [];
for (let i = 0; i < 1000; i++) {
    arr.push(1000 - i);
}

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
console.timeEnd("JS Bubble Sort");
console.log(`JavaScript: First: ${arr[0]}, Last: ${arr[999]}`);
