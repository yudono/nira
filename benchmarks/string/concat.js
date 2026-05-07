let start = Date.now();
let s = "";
for (let i = 0; i < 10000000; i++) {
    s += "a";
}
let end = Date.now();
console.log(`JavaScript: ${end - start} ms (Length: ${s.length})`);
