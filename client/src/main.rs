use std::env;
use std::io;
use std::sync::{Arc, Mutex};
use std::thread;
use std::usize;

use handler::{handle_file_actions, handle_network_communication};
use ratatui::buffer;
use ratatui::{backend::CrosstermBackend, Terminal};
use tokio::net::TcpStream;
use tokio::sync::mpsc;

use crate::{
    app::{App, AppResult},
    event::{Event, EventHandler},
    handler::handle_key_events,
    lib::Playback,
    tui::Tui,
};

pub mod app;
pub mod event;
pub mod handler;
pub mod lib;
pub mod tui;
pub mod ui;

const CHUNK_SIZE: usize = 4096 * 8;

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

    let (tx, rx) = mpsc::channel(32);

    let playback_handle = {
        let rx = Arc::new(Mutex::new(rx));
        thread::spawn(move || Playback::playback_audio(rx))
    };

    // Start independent thread for audio playback
    let network_handle = {
        let a_stream = Arc::new(Mutex::new(a_stream));
        let tx = tx.clone();
        thread::spawn(move || loop {
            {
                let mut buffer = vec![0; CHUNK_SIZE];
                let mut lock_stream = a_stream.lock().unwrap();
                match lock_stream.try_read(&mut buffer) {
                    Ok(size) if size > 0 => {
                        if tx.blocking_send(buffer).is_err() {
                            eprintln!("Failed to send audio to playback_handle");
                            break;
                        }
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
        })
    };

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

    playback_handle.join().unwrap();
    network_handle.join().unwrap();
    // Exit the user interface.
    tui.exit()?;
    Ok(())
}
