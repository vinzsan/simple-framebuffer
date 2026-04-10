## simple-framebuffer
simple-framebuffer, simple and fast `/dev/fb0`

### what kind of mango is this?

Framebuffer adalah bagian dari memori RAM (seringkali pada kartu grafis/GPU) yang berfungsi menyimpan bitmap data gambar lengkap sebelum ditampilkan ke layar. Ia bertindak sebagai perantara yang memetakan piksel dalam memori ke piksel di monitor, memungkinkan rendering grafis yang efisien dan memisahkan prosesor dari detail layar

> unit test

<video src="assets/record_bit_frame_1.mp4" controls width="600"></video>

- features
    - [x] window rendering
    - [x] ttf font load
    - [x] backbuffer to fflush
    - [ ] texture loader
    - [ ] audio decoder

> **Note**: bits per pixel mungkin tidak akurat

