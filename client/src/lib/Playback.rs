use rodio::{Decoder, Sink};
use std::collections::VecDeque;
use std::io::Cursor;
use std::sync::Arc;
use std::u8;
use tokio::sync::{mpsc, Mutex};
use tokio::time::{sleep, Duration};

pub async fn playback_audio(rx: Arc<Mutex<mpsc::Receiver<Vec<u8>>>>, sink: Arc<Sink>) {
    let mut playback_buffer: VecDeque<Vec<u8>> = VecDeque::new();
    let min_buf = 2;
    loop {
        let chunk = rx.lock().await.recv().await;
        match chunk {
            Some(data) => {
                playback_buffer.push_back(data);
                if playback_buffer.len() >= min_buf {
                    let audio_chunk = playback_buffer.pop_front();
                    match audio_chunk {
                        Some(ac) => {
                            let cursor = Cursor::new(ac);
                            if let Ok(source) = Decoder::new(cursor) {
                                sink.append(source);
                            }
                        }
                        None => {}
                    }
                }
            }
            None => {
                // tx was shutdown kill thread
                break;
            }
        }
    }
}
