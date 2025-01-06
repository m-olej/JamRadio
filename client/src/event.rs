use std::time::Duration;
use std::sync::{Arc, Mutex};
use crossterm::event::{Event as CrosstermEvent, KeyEvent, MouseEvent};
use crossterm::event::KeyCode;
use futures::{FutureExt, StreamExt};
use tokio::net::TcpStream;
use tokio::sync::mpsc;

use crate::app::AppResult;

/// Terminal events.
#[derive(Clone, Debug)]
pub enum Event {
    /// Terminal tick.
    Tick,
    /// Key press.
    Key(KeyEvent),
    /// Mouse click/scroll.
    Mouse(MouseEvent),
    /// Terminal resize.
    Resize(u16, u16),
    /// Server update network communication
    Net([u8; 1024]),
    /// File Transfer 
    FileTransfer,
}

/// Terminal event handler.
#[allow(dead_code)]
#[derive(Debug)]
pub struct EventHandler {
    /// Event sender channel.
    sender: mpsc::UnboundedSender<Event>,
    /// Event receiver channel.
    receiver: mpsc::UnboundedReceiver<Event>,
    /// Event handler thread.
    handler: tokio::task::JoinHandle<()>,
}

impl EventHandler {
    /// Constructs a new instance of [`EventHandler`].
    pub fn new(tick_rate: u64, stream: Arc<Mutex<TcpStream>>) -> Self {
        let tick_rate = Duration::from_millis(tick_rate);
        let (sender, receiver) = mpsc::unbounded_channel();
        let _sender = sender.clone();
        let handler = tokio::spawn(async move {
            let mut reader = crossterm::event::EventStream::new();
            let mut tick = tokio::time::interval(tick_rate);
            loop {
                let tick_delay = tick.tick();
                let crossterm_event = reader.next().fuse();
                let mut update_buf = [0; 1024];
                tokio::select! {
                  _ = _sender.closed() => {
                    break;
                  }
                  _ = tick_delay => {
                    _sender.send(Event::Tick).unwrap();
                  }
                result = async { stream.lock().unwrap().try_read(&mut update_buf) } => {
                    match result {
                            Ok(0) => {
                                // connection closed
                                continue;
                            }
                            Ok(_n) => {
                                let data = update_buf;
                                let _ = _sender.send(Event::Net(data));
                                // If n == 1024 check for rest of message if neccessary
                            }
                            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                                // no new data in socket
                                continue;
                            }
                            Err(err) => {
                            eprintln!("Failed to read from stream: {}", err);
                            }
                        }
                }
                Some(Ok(evt)) = crossterm_event => {
                        
                    match evt {
                      CrosstermEvent::Key(key) => {
                        if key.kind == crossterm::event::KeyEventKind::Press {
                            if key.code == KeyCode::Enter {
                                _sender.send(Event::FileTransfer).unwrap();
                            } else {
                                _sender.send(Event::Key(key)).unwrap();
                            } 
                        }
                      },
                      CrosstermEvent::Mouse(mouse) => {
                        _sender.send(Event::Mouse(mouse)).unwrap();
                      },
                      CrosstermEvent::Resize(x, y) => {
                        _sender.send(Event::Resize(x, y)).unwrap();
                      },
                      CrosstermEvent::FocusLost => {
                      },
                      CrosstermEvent::FocusGained => {
                      },
                      CrosstermEvent::Paste(_) => {
                      },
                    }
                  }
                };
            }
        });
        Self {
            sender,
            receiver,
            handler,
        }
    }

    /// Receive the next event from the handler thread.
    ///
    /// This function will always block the current thread if
    /// there is no data available and it's possible for more data to be sent.
    pub async fn next(&mut self) -> AppResult<Event> {
        self.receiver
            .recv()
            .await
            .ok_or(Box::new(std::io::Error::new(
                std::io::ErrorKind::Other,
                "This is an IO error",
            )))
    }
}
