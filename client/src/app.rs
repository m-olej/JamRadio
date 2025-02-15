use crate::lib::FileExplorer;
use crate::lib::{
    FileExplorer::get_dir_contents,
    NetUtils::{self, sendSong},
};
use ratatui::widgets::ListState;
use serde::{Deserialize, Serialize};
use std::{
    error,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc, Mutex,
    },
};
use tokio::io::AsyncWriteExt;
use tokio::net::TcpStream;

/// Application result type.
pub type AppResult<T> = std::result::Result<T, Box<dyn error::Error>>;

// Information given with server updates
#[derive(Deserialize, Serialize, Debug)]
pub struct ServerState {
    pub active_listeners: u8,
    pub song_library: Vec<String>,
    pub song_queue: Vec<String>,
}

impl Default for ServerState {
    fn default() -> Self {
        Self {
            active_listeners: 0,
            song_library: vec![],
            song_queue: vec![],
        }
    }
}

/// Application.
#[derive(Debug)]
pub struct App<'a> {
    /// Is the application running?
    pub running: Arc<AtomicBool>,
    /// TCP communication connection
    pub c_connection: Option<Arc<Mutex<TcpStream>>>,
    /// TCP audio connection
    pub a_connection: Option<Arc<Mutex<TcpStream>>>,
    /// Clients File Explorer State
    pub client_fs_state: ListState,
    /// Servers File Explorer State
    pub server_fs_state: ListState,
    /// Which file explorer is selected
    /// 0 -> server | 1 -> client
    pub client_fs_selected: bool,
    /// State received from server
    pub state: ServerState,

    /// CONSTANTS
    pub song_dir: &'a str,
}

impl<'a> Default for App<'a> {
    fn default() -> Self {
        Self {
            running: Arc::new(AtomicBool::new(true)),
            c_connection: None,
            a_connection: None,
            client_fs_state: ListState::default().with_selected(Some(0)),
            server_fs_state: ListState::default(),
            client_fs_selected: true,
            state: ServerState::default(),
            song_dir: "./songs/",
        }
    }
}

impl<'a> App<'a> {
    /// Constructs a new instance of [`App`].
    pub fn new() -> Self {
        Self::default()
    }

    pub fn add_comm_connection(&mut self, connection: TcpStream) {
        self.c_connection = Some(Arc::new(Mutex::new(connection)));
    }

    pub fn add_audio_connection(&mut self, connection: TcpStream) {
        self.a_connection = Some(Arc::new(Mutex::new(connection)));
    }

    pub fn get_comm_connection(&mut self) -> &mut Arc<Mutex<TcpStream>> {
        self.c_connection.as_mut().unwrap()
    }

    pub fn get_audio_connection(&mut self) -> &mut Arc<Mutex<TcpStream>> {
        self.a_connection.as_mut().unwrap()
    }

    pub fn get_client_song_path(&mut self) -> String {
        let selected_file = self.client_fs_state.selected().unwrap();
        let items = get_dir_contents(self.song_dir);

        let mut file_path = items.get(selected_file).unwrap().to_string();

        file_path.insert_str(0, self.song_dir);

        file_path
    }

    pub fn get_song(&mut self) -> String {
        let selected_song = self.server_fs_state.selected().unwrap();

        self.state.song_library[selected_song].clone()
    }

    /// Handles the tick event of the terminal.
    pub fn tick(&self) {}

    /// Set running to false to quit the application.
    pub fn quit(&mut self) {
        self.running.store(false, Ordering::Relaxed);
        println!("app detected quit");
        if let Some(conn) = self.c_connection.take() {
            let _ = conn.lock().unwrap().shutdown();
            println!("C_connection shutdown");
        }
    }

    pub fn handle_fs_state(&mut self, direction: &str) {
        if self.client_fs_selected {
            if direction == "down" {
                self.client_fs_state.select_next();
            } else {
                self.client_fs_state.select_previous();
            }
        } else {
            if direction == "down" {
                self.server_fs_state.select_next();
            } else {
                self.server_fs_state.select_previous();
            }
        }
    }

    pub fn switch_fs(&mut self) {
        self.client_fs_selected = !self.client_fs_selected;
        if self.client_fs_selected {
            *self.server_fs_state.selected_mut() = None;
            *self.client_fs_state.selected_mut() = Some(0);
        } else {
            *self.server_fs_state.selected_mut() = Some(0);
            *self.client_fs_state.selected_mut() = None;
        }
    }

    pub fn update_state(&mut self, state: ServerState) {
        self.state = state;
    }

    pub fn handle_fs_actions(&mut self) {
        if self.client_fs_selected {
            println!("Send file to server");
        } else {
            println!("Add song to server playback queue");
        }
    }
}
