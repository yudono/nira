Berikut adalah beberapa skenario pengujian komprehensif untuk menguji ketangguhan bahasa pemrograman Anda:

---

## 1. High-Precision Arithmetic (Stress Test)

Tes ini bertujuan untuk melihat apakah bahasa Anda mengalami _overflow_ atau kehilangan akurasi saat menangani angka yang sangat besar atau sangat kecil.

### Integer High Precision (The Factorial Test)

Hitunglah faktorial dari angka besar (misalnya 1000!). Jika bahasa Anda mendukung _arbitrary-precision integers_, hasilnya harus tepat tanpa _overflow_.

- **Target:** Kemampuan alokasi memori dinamis untuk tipe data integer.
- **Contoh Logika:**
  `result = 1; for i from 1 to 1000: result = result * i; print result`

### Floating Point High Precision (The Pi Test)

Gunakan algoritma Chudnovsky atau deret Leibniz untuk menghitung nilai $\pi$ hingga ribuan digit di belakang koma.

- **Target:** Menguji pembulatan (_rounding errors_) dan presisi mantissa.
- **Contoh Logika:** Hitung $\sum_{n=0}^{\infty} \frac{(-1)^n}{2n+1}$ dan bandingkan tingkat konvergensinya dengan konstanta sistem.

---

## 2. Memory & Recursion (The Stability Test)

Bagian ini menguji bagaimana bahasa Anda menangani _stack_ dan _heap memory_.

### Deep Recursion (The Ackermann Function)

Fungsi Ackermann adalah fungsi matematika yang tumbuh sangat cepat dan memaksa penggunaan _stack_ yang sangat dalam.

- **Target:** Menguji optimasi _tail-call_ atau batas _stack overflow_.
- **Fungsi:**
  $$
  A(m, n) =
  \begin{cases}
  n + 1 & \text{if } m = 0 \\
  A(m-1, 1) & \text{if } m > 0 \text{ and } n = 0 \\
  A(m-1, A(m, n-1)) & \text{if } m > 0 \text{ and } n > 0
  \end{cases}
  $$

### Massive Object Allocation (Garbage Collection Test)

Buatlah _loop_ yang menginstansiasi 1 juta objek kecil di dalam list, lalu hapus list tersebut, dan ulangi 100 kali.

- **Target:** Jika bahasa Anda memiliki _Garbage Collector_ (GC), tes ini akan menunjukkan apakah ada kebocoran memori (_memory leak_).

---

## 3. Stress Test Struktur Data

Menguji efisiensi pencarian dan manipulasi data dalam jumlah besar.

- **Nested Collections:** Buatlah sebuah Map/Dictionary yang berisi List, yang di dalamnya terdapat Map lagi, hingga kedalaman 50 level. Coba akses data di level terdalam.
- **Sort Large Array:** Buat array berisi 1 juta angka acak dan urutkan menggunakan algoritma bawaan bahasa Anda. Ini menguji kecepatan eksekusi dan stabilitas _pointer_.

---

## 4. Edge Cases & Error Handling

Bahasa yang stabil bukan hanya yang cepat, tapi yang tidak "crash" saat diberi input aneh.

| Skenario                | Deskripsi                                                                                                                         |
| :---------------------- | :-------------------------------------------------------------------------------------------------------------------------------- |
| **Division by Zero**    | Pastikan sistem melempar _exception_ yang jelas, bukan _segmentation fault_.                                                      |
| **Empty Strings/Lists** | Coba lakukan operasi `pop()` atau `slice` pada list kosong.                                                                       |
| **Concurrency Race**    | Jika bahasa mendukung multi-threading, jalankan 1000 thread yang mencoba menambah nilai ke satu variabel global secara bersamaan. |
| **Unicode Stress**      | Gunakan karakter Emoji, aksara Arab, atau Kanji sebagai nama variabel atau isi string.                                            |

---

## 5. I/O Stabilitas

Coba buat program yang membaca dan menulis file berukuran 2GB secara terus-menerus dalam potongan kecil (_buffers_). Ini akan menunjukkan apakah bahasa Anda mampu menangani _Resource Management_ dengan baik tanpa membuat sistem operasi "marah".
