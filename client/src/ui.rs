use ratatui::{
    layout::{Alignment, Constraint, Direction, Layout},
    style::{Color, Style},
    widgets::{Block, BorderType, Gauge, Paragraph},
    Frame,
};

use crate::app::App;

// Custom widgets

// client file explorer

pub struct SongProgress {} // Status bar of song lenght [Gauge]
pub struct ServerSongLibrary {} // Custom List from Server Song Library info
pub struct SongCard {} // Song Card

/// Renders the user interface widgets.
pub fn render(app: &mut App, frame: &mut Frame) {
    // This is where you add new widgets.
    // See the following resources:
    // - https://docs.rs/ratatui/latest/ratatui/widgets/index.html
    // - https://github.com/ratatui/ratatui/tree/master/examples

    let default_style = Style::default().fg(Color::Cyan).bg(Color::Black);

    // Creating the layout in assets/tui_desing.jpeg
    let main_layout = Layout::default()
        .direction(Direction::Horizontal)
        .constraints(vec![Constraint::Percentage(75), Constraint::Percentage(25)])
        .split(frame.area());
    let functional_layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints(vec![Constraint::Fill(1), Constraint::Length(5)])
        .split(main_layout[0]);
    let queue_layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints(vec![Constraint::Length(5), Constraint::Fill(1)]) // Cons
        .split(main_layout[1]);
    let fs_layout = Layout::default()
        .direction(Direction::Horizontal)
        .constraints(vec![Constraint::Percentage(50), Constraint::Percentage(50)])
        .split(functional_layout[0]);
    let server_layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints(vec![Constraint::Length(5), Constraint::Fill(1)])
        .split(fs_layout[0]);

    // Server File Explorer
    frame.render_widget(
        Paragraph::new(format!("Active listeners: {}", 0))
            .block(Block::bordered().border_type(BorderType::Rounded))
            .style(default_style),
        server_layout[0],
    );
    frame.render_widget(
        Paragraph::new(format!(
            "This is a tui template.\n\
                Press `Esc`, `Ctrl-C` or `q` to stop running.\n\
                Press left and right to increment and decrement the counter respectively.\n\
                Counter: {}",
            app.counter
        ))
        .block(
            Block::bordered()
                .title("Template")
                .title_alignment(Alignment::Center)
                .border_type(BorderType::Rounded),
        )
        .style(default_style)
        .centered(),
        server_layout[1],
    );

    // Client File Explorer
    frame.render_widget(&app.client_file_explorer.widget(), fs_layout[1]);

    // Queue
    frame.render_widget(
        Paragraph::new("JamQueue")
            .block(Block::bordered().border_type(BorderType::Rounded))
            .style(default_style),
        queue_layout[0],
    );
    frame.render_widget(
        Paragraph::new("To be added")
            .block(Block::bordered().border_type(BorderType::Rounded))
            .style(default_style),
        queue_layout[1],
    );

    // Audio progress bar
    frame.render_widget(
        Gauge::default()
            .block(
                Block::bordered()
                    .title("Song progress")
                    .border_type(BorderType::Rounded)
                    .title_alignment(Alignment::Center),
            )
            .style(default_style),
        functional_layout[1],
    );
}
