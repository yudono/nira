console.time("JS String Concat");
let s = "";
for (let i = 0; i < 10000; i++) {
    s += "a";
}
console.timeEnd("JS String Concat");
console.log(`JavaScript: Length: ${s.length}`);
