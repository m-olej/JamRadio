use crate::app::{App, AppResult};
use std::sync::{Arc, Mutex};
use tokio::fs::File;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;

// SongTransfer protocol [signature 'f' at buf[0]]
// filename_size -> 4B
// filename -> var
// audio_file_size -> 4B
// audi_file -> var

#[allow(non_snake_case)]
pub async fn sendSong<'a>(file_path: String, app: &mut App<'a>) -> AppResult<()> {
    let stream: Arc<Mutex<TcpStream>> = app.c_connection.clone().unwrap();

    // Initialize message with signature
    let mut message: Vec<u8> = "f".into();

    // filename_size
    let filename_size = file_path.len() as u32; // 4B

    // filename
    let path_buffer: Vec<u8> = file_path.clone().into();

    // audio_file
    let mut file = File::open(file_path).await?;
    let mut buffer: Vec<u8> = Vec::new();
    file.read_to_end(&mut buffer).await?;

    // audio_file_size
    let file_size = buffer.len() as u32; // 4B

    // Compose message
    message.extend(&filename_size.to_be_bytes());
    message.extend(path_buffer);
    message.extend(&file_size.to_be_bytes());
    message.extend(buffer);

    stream.lock().unwrap().write_all(&message).await?;

    Ok(())
}
