use crate::{app::{App, AppResult, ServerState}, lib::NetUtils::sendSong};
use crossterm::event::{KeyCode, KeyEvent, KeyModifiers};

/// Handles the key events and updates the state of [`App`].
pub fn handle_key_events(key_event: KeyEvent, app: &mut App) -> AppResult<()> {
    match key_event.code {
        // Exit application on `ESC` or `q`
        KeyCode::Esc | KeyCode::Char('q') => {
            app.quit();
        }
        // Exit application on `Ctrl-C`
        KeyCode::Char('c') | KeyCode::Char('C') => {
            if key_event.modifiers == KeyModifiers::CONTROL {
                app.quit();
            }
        }
        KeyCode::Up => {
            app.handle_fs_state("up");
        }
        KeyCode::Down => {
            app.handle_fs_state("down");
        }
        KeyCode::Tab => {
            app.switch_fs();
        }
        KeyCode::Enter => {
            app.handle_fs_actions();
        }
        _ => {}
    }
    Ok(())
}

pub fn handle_network_communication(message: &[u8; 1024], app: &mut App) -> AppResult<()> {
    // Convert array of u8 to json string
    let trimmed_message = message.split(|&byte| byte == 0).next().unwrap_or(message);
    let update_string = match std::str::from_utf8(trimmed_message) {
        Ok(update_string) => update_string.trim(),
        Err(err) => {
            eprintln!("Failed to parse message: {}", err);
            "ki chuj"
        }
    };

    // convert json String to ServerState data strucure
    let server_state: ServerState = serde_json::from_str(update_string).unwrap();

    app.update_state(server_state);

    Ok(())
}

pub async fn handle_file_actions(app: &mut App<'_>) -> AppResult<()> {
    
    if app.client_fs_selected {
        let file_path = app.get_client_song_path();
        sendSong(file_path, app).await?;
    } else {
        // Send add song to queue request
        println!("Not implemented yet")
    }

    Ok(())
}
