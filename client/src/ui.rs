use ratatui::{
    layout::{Alignment, Constraint, Direction, Layout},
    style::{Color, Style, Stylize},
    widgets::{Block, BorderType, Gauge, List, ListDirection, Paragraph},
    Frame,
};

use crate::app::App;
use crate::lib::FileExplorer;

// Custom widgets

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
        Paragraph::new(format!(
            "Active listeners: {}",
            app.state.active_listeners.to_string()
        ))
        .block(Block::bordered().border_type(BorderType::Rounded))
        .style(default_style),
        server_layout[0],
    );

    // ToDo: Get songs from server via async Tokio receiver
    let server_items = app.state.song_library.clone();

    let server_list = List::new(server_items);

    frame.render_stateful_widget(
        server_list
            .block(
                Block::bordered()
                    .title("Songs to send")
                    .border_type(BorderType::Rounded)
                    .title_alignment(Alignment::Center),
            )
            .style(default_style)
            .highlight_style(Style::new().italic())
            .highlight_symbol(">> ")
            .repeat_highlight_symbol(true)
            .direction(ListDirection::TopToBottom),
        server_layout[1],
        &mut app.server_fs_state,
    );

    let client_items = FileExplorer::get_dir_contents(app.song_dir);

    let client_list = List::new(client_items);

    // Client File Explorer
    frame.render_stateful_widget(
        client_list
            .block(
                Block::bordered()
                    .title("Songs to send")
                    .border_type(BorderType::Rounded)
                    .title_alignment(Alignment::Center),
            )
            .style(default_style)
            .highlight_style(Style::new().italic())
            .highlight_symbol(">> ")
            .repeat_highlight_symbol(true)
            .direction(ListDirection::TopToBottom),
        fs_layout[1],
        &mut app.client_fs_state,
    );

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
