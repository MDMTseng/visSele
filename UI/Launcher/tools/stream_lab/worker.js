// Decode a JPEG off the main thread and hand back a VideoFrame by TRANSFER.
// A VideoFrame is transferable, so its pixels never cross the thread boundary
// -- the main thread receives a handle and uploads it straight to the GPU.
self.onmessage = async (e) => {
  const { id, bytes } = e.data;
  try {
    const dec = new ImageDecoder({ data: bytes, type: 'image/jpeg' });
    const { image } = await dec.decode();
    dec.close();
    self.postMessage({ id, frame: image }, [image]);
  } catch (err) {
    self.postMessage({ id, error: String(err && err.message || err) });
  }
};
