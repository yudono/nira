# 🧠 Nira Development Memory

## 📑 Recap: Apa yang Telah Terjadi?

Kami baru saja menyelesaikan fase penguatan (**hardening**) pada arsitektur runtime dan kompiler Nira. Fokus utama adalah stabilitas memori dan konsistensi antara mode interpretasi (`nira run`) dan kompilasi (`nira build`).

### 1. Sistem Memori (Arena Allocator)
- **Integrasi Penuh**: Arena Allocator kini menjadi tulang punggung manajemen memori di interpreter (`evaluator.c`) dan biner hasil kompilasi (`codegen_c.c`).
- **Zero Leak Strategy**: Implementasi mekanisme `checkpoint` dan `rollback` untuk memastikan memori dibersihkan setelah setiap *request* (di server HTTP) atau di akhir eksekusi program.
- **Thread Safety (Initial)**: Struktur Arena dirancang untuk mudah dikembangkan ke model per-thread di masa depan.

### 2. Perbaikan Parser & Sintaks
- **Keyword `print`**: Menjadikan `print` sebagai kata kunci utama (*first-class keyword*). Ini menghilangkan ambiguitas saat menggunakan sintaks `print "pesan"` tanpa tanda kurung.
- **Fix Deteksi Float**: Memperbaiki bug kritis di mana parser salah mendeteksi angka bulat sebagai float karena adanya titik di baris-baris berikutnya pada file sumber.
- **Akurasi AST**: Memperbaiki `ast_new` untuk menggunakan token yang tepat, sehingga pelacakan baris/kolom pada pesan error menjadi jauh lebih akurat.

### 3. Paritas Fungsional Kompiler
- **Dukungan Literal Float**: Menambahkan penanganan `AST_LITERAL_FLOAT` di generator kode C yang sebelumnya hilang.
- **Stabilitas Objek & Array**: Memperbaiki korupsi data pada inisialisasi literal objek dalam kode C yang dihasilkan.

---

## 🚀 Apa yang Akan Dilakukan?

Langkah selanjutnya akan fokus pada perluasan fungsionalitas dan optimalisasi performa.

### 1. Ekspansi Standard Library (StdLib)
- **Arena-Backed Strings**: Memastikan semua fungsi manipulasi string di `stdlib` menggunakan memori Arena sepenuhnya.
- **Networking & File I/O**: Memperluas library `http` dan `file` agar lebih tangguh dalam menangani *stress test* memori tinggi.

### 2. Optimalisasi Performa
- **Block Reuse**: Mengimplementasikan logika penggunaan kembali blok memori yang sudah di-*free* dalam Arena untuk mengurangi frekuensi `malloc` sistem.
- **Escape Analysis (Future)**: Mulai riset awal untuk *Escape Analysis* agar objek yang tidak "keluar" dari scope fungsi bisa dialokasikan di *stack* secara otomatis.

### 3. Tooling & DX (Developer Experience)
- **Improved Error Messaging**: Menggunakan data koordinat AST yang baru diperbaiki untuk memberikan saran perbaikan kode yang lebih cerdas (seperti Rust *suggestions*).
- **Benchmarking Suite**: Membuat skrip pengujian performa otomatis untuk membandingkan kecepatan interpreter vs hasil kompilasi secara berkala.

---
*Last Updated: 2026-05-07*
