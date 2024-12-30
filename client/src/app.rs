use std::{char, error};

use ratatui::widgets::ListState;

/// Application result type.
pub type AppResult<T> = std::result::Result<T, Box<dyn error::Error>>;

// Information given with server updates
#[derive(Debug)]
pub struct ServerState {
    pub active_listeners: char,
}

impl Default for ServerState {
    fn default() -> Self {
        Self {
            active_listeners: '0',
        }
    }
}

/// Application.
#[derive(Debug)]
pub struct App {
    /// Is the application running?
    pub running: bool,
    /// counter
    pub counter: u8,
    /// Clients File Explorer State
    pub client_fs_state: ListState,
    /// Servers File Explorer State
    pub server_fs_state: ListState,
    /// Which file explorer is selected
    /// 0 -> server | 1 -> client
    pub client_fs_selected: bool,

    pub state: ServerState,
}

impl Default for App {
    fn default() -> Self {
        Self {
            running: true,
            counter: 0,
            client_fs_state: ListState::default(),
            server_fs_state: ListState::default(),
            client_fs_selected: true,
            state: ServerState::default(),
        }
    }
}

impl App {
    /// Constructs a new instance of [`App`].
    pub fn new() -> Self {
        Self::default()
    }

    /// Handles the tick event of the terminal.
    pub fn tick(&self) {}

    /// Set running to false to quit the application.
    pub fn quit(&mut self) {
        self.running = false;
    }

    pub fn increment_counter(&mut self) {
        if let Some(res) = self.counter.checked_add(1) {
            self.counter = res;
        }
    }

    pub fn decrement_counter(&mut self) {
        if let Some(res) = self.counter.checked_sub(1) {
            self.counter = res;
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
}
