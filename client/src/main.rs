use std::io;
use std::env;

use handler::{handle_network_communication, handle_file_actions};
use ratatui::{backend::CrosstermBackend, Terminal};
use tokio::net::TcpStream;

use crate::{
    app::{App, AppResult},
    event::{Event, EventHandler},
    handler::handle_key_events,
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
    
    println!("{}", server_comm_connection_string);
    println!("{}", server_audio_connection_string);

    // Initialize the terminal user interface.
    let backend = CrosstermBackend::new(io::stdout());
    let terminal = Terminal::new(backend)?;
    let c_stream: TcpStream = TcpStream::connect(server_comm_connection_string).await?;
    let a_stream: TcpStream = TcpStream::connect(server_audio_connection_string).await?;
    app.add_comm_connection(c_stream);
    app.add_audio_connection(a_stream); 
    let connection = app.c_connection.clone().unwrap();

    let events = EventHandler::new(250, connection.clone());
    let mut tui = Tui::new(terminal, events);
    tui.init()?;

    // Start the main loop.
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
