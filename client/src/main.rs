use std::env;
use std::io;
use std::thread;

use handler::{handle_file_actions, handle_network_communication};
use ratatui::{backend::CrosstermBackend, Terminal};
use rodio::{OutputStream, Sink};
use tokio::net::TcpStream;

use crate::{
    app::{App, AppResult},
    event::{Event, EventHandler},
    handler::handle_key_events,
    lib::RawAudioSource::RawAudioSource,
    tui::Tui,
};

pub mod app;
pub mod event;
pub mod handler;
pub mod lib;
pub mod tui;
pub mod ui;

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

    // Start independent thread for audio playback
    thread::spawn(move || {
        let (_stream, stream_handle) = OutputStream::try_default().unwrap();
        let sink = Sink::try_new(&stream_handle).unwrap();
        let audio_source = RawAudioSource::new();
        sink.append(audio_source.clone());
        loop {
            let mut buffer = [0; 4096]; // 4KB
            match a_stream.try_read(&mut buffer) {
                Ok(size) if size > 0 => {
                    let chunk: Vec<i16> = buffer[..size]
                        .chunks(2)
                        .filter_map(|b| b.try_into().ok())
                        .map(i16::from_le_bytes)
                        .collect();
                    audio_source.append_data(chunk);
                    break;
                }
                Ok(_) => {
                    continue;
                }
                Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                    // no new data in socket
                    continue;
                }
                Err(err) => {
                    eprintln!("Failed to read from stream: {}", err);
                    break;
                }
            };
        }
    });

    // Start the TUI loop.
    while app.running {
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

    // Exit the user interface.
    tui.exit()?;
    Ok(())
}
