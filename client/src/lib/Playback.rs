use rodio::{Decoder, OutputStream, Sink};
use std::io::Cursor;
use std::sync::{Arc, Mutex};
use tokio::sync::mpsc;

pub fn playback_audio(rx: Arc<Mutex<mpsc::Receiver<Vec<u8>>>>) {
    let (_stream, stream_handle) = OutputStream::try_default().unwrap();
    let sink = Sink::try_new(&stream_handle).unwrap();

    while let chunk = rx.lock().unwrap().blocking_recv().unwrap() {
        // Create a cursor for the audio data
        let cursor = Cursor::new(chunk);

        // Decode audio data and play
        if let Ok(source) = Decoder::new(cursor) {
            sink.append(source);
            sink.play();
        } else {
            eprintln!("Failed to decode audio chunk");
        }
    }
}
