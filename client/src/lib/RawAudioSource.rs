use rodio::{OutputStream, Sink, Source};
use std::sync::{Arc, Mutex};
use std::time::Duration;

pub struct RawAudioSource {
    pub data: Arc<Mutex<Vec<i16>>>, // Shared buffer for raw audio samples
    pub position: usize,            // Current read position in the buffer
}

impl RawAudioSource {
    pub fn new() -> Self {
        RawAudioSource {
            data: Arc::new(Mutex::new(Vec::new())),
            position: 0,
        }
    }

    pub fn append_data(&self, chunk: Vec<i16>) {
        let mut data = self.data.lock().unwrap();
        data.extend(chunk);
    }
}

impl Iterator for RawAudioSource {
    type Item = i16;

    fn next(&mut self) -> Option<Self::Item> {
        let data = self.data.lock().unwrap();
        if self.position < data.len() {
            let sample = data[self.position];
            self.position += 1;
            Some(sample)
        } else {
            None
        }
    }
}

impl Clone for RawAudioSource {
    fn clone(&self) -> Self {
        RawAudioSource {
            data: Arc::clone(&self.data),
            position: self.position.clone(),
        }
    }
}

impl Source for RawAudioSource {
    fn current_frame_len(&self) -> Option<usize> {
        None // Infinite stream
    }

    fn channels(&self) -> u16 {
        2 // Stereo
    }

    fn sample_rate(&self) -> u32 {
        44100 // Sample rate in Hz
    }

    fn total_duration(&self) -> Option<Duration> {
        None // Infinite stream
    }
}

// Example use
// fn main() {
// let (_stream, stream_handle) = OutputStream::try_default().unwrap();
// let sink = Sink::try_new(&stream_handle).unwrap();

// Create the raw audio source
// let raw_source = RawAudioSource::new();

// Start playback
// sink.append(raw_source.clone());

// Simulate streaming raw PCM audio data (hardcoded to stereo, 16-bit, 44.1 kHz)
// std::thread::spawn({
// let raw_source = raw_source.clone();
// move || {
// Simulate streaming chunks
// for _ in 0..5 {
// Generate or load raw PCM samples (e.g., sine wave or from a file)
// let chunk: Vec<i16> = vec![0; 44100 * 2]; // 1 second of silence
// raw_source.append_data(chunk);
// std::thread::sleep(std::time::Duration::from_secs(1));
// }
// }
// });

// sink.sleep_until_end();
// }
