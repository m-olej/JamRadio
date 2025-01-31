use std::env;
use std::io;
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};
use std::usize;

use handler::{handle_file_actions, handle_network_communication};
use ratatui::{backend::CrosstermBackend, Terminal};
use tokio::io::{AsyncReadExt, BufReader};
use tokio::net::TcpStream;
use tokio::sync::{mpsc, Mutex, Notify};

use crate::{
    app::{App, AppResult},
    event::{Event, EventHandler},
    handler::handle_key_events,
    lib::Playback,
    tui::Tui,
};

use ::rodio::{OutputStream, Sink};

pub mod app;
pub mod event;
pub mod handler;
pub mod lib;
pub mod tui;
pub mod ui;

const CHUNK_SIZE: usize = 10000;

#[tokio::main]
async fn main() -> AppResult<()> {
    // Create an application.
    let mut app = App::new();

    let args: Vec<String> = env::args().collect();

    let server_comm_connection_string: String = format!("{}:{}", &args[1], &args[2]);
    let server_audio_connection_string: String = format!("{}:{}", &args[1], &args[3]);

    // Initialize the terminal user interface.
    let backend = CrosstermBackend::new(io::stdout());
    let terminal = Terminal::new(backend)?;
    let c_stream: TcpStream = TcpStream::connect(server_comm_connection_string).await?;
    let a_stream: TcpStream = TcpStream::connect(server_audio_connection_string).await?;
    app.add_comm_connection(c_stream);
    // app.add_audio_connection(a_stream);
    let connection = app.c_connection.clone().unwrap();

    let events = EventHandler::new(250, connection.clone());
    let mut tui = Tui::new(terminal, events);
    tui.init()?;

    let (tx, rx): (mpsc::Sender<Vec<u8>>, mpsc::Receiver<Vec<u8>>) = mpsc::channel(32);
    let rx = Arc::new(Mutex::new(rx));

    let (_stream, stream_handle) = OutputStream::try_default().unwrap();
    let sink = Arc::new(Sink::try_new(&stream_handle).unwrap());

    tokio::spawn({
        let rx = Arc::clone(&rx);
        let sink = Arc::clone(&sink);
        async move {
            Playback::playback_audio(rx, sink).await;
        }
    });

    let shutdown_notify = Arc::new(Notify::new());

    let shutdown_signal = shutdown_notify.clone();
    tokio::spawn(async move {
        let tx = tx.clone(); // Clone the channel sender.
        let mut buffer = vec![0; CHUNK_SIZE];
        let mut reader = BufReader::new(a_stream);
        loop {
            // Perform the actual read from the stream
            tokio::select! {
            result = reader.read(&mut buffer) => {
            match result {
                Ok(0) => {
                    // Stream closed
                    eprintln!("Stream closed");
                    break;
                }
                Ok(size) => {
                    // Truncate to the actual read size before sending
                    buffer.truncate(size);
                    if let Err(e) = tx.send(buffer.clone()).await {
                        eprintln!("Failed to send audio to playback: {}", e);
                        break;
                    }
                }
                Err(e) => {
                    eprintln!("Failed to read from stream: {}", e);
                    break;
                }
            }}
            _ = shutdown_signal.notified() => {
                        drop(tx);
                        break;
                }
            }
        }
    });

    // Start the TUI loop.
    while app.running.load(Ordering::Relaxed) {
        // Render the user interface.
        tui.draw(&mut app)?;
        // Handle events.
        match tui.events.next().await? {
            Event::Tick => app.tick(),
            Event::Key(key_event) => handle_key_events(key_event, &mut app)?,
            Event::Mouse(_) => {}
            Event::Resize(_, _) => {}
            Event::Net(message) => handle_network_communication(&message, &mut app)?,
            Event::FileTransfer => handle_file_actions(&mut app).await?,
        }
    }
    shutdown_notify.notify_waiters();
    // Exit the user interface.
    tui.exit()?;
    Ok(())
}
