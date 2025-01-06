use std::sync::{Arc, Mutex};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::fs::File;
use crate::app::{App, AppResult};

#[allow(non_snake_case)]
pub async fn sendSong<'a>(file_path: String, app: &mut App<'a>) -> AppResult<()> {
    let stream: Arc<Mutex<TcpStream>> = app.connection.clone().unwrap();

    let mut file = File::open(file_path).await?;

    let mut buffer: Vec<u8> = Vec::new();

    file.read_to_end(&mut buffer).await?;
   
    stream.lock().unwrap().write_all(&buffer).await?;
    
    Ok(())
}
