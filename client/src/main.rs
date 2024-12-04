use std::io::{Read, Write};
use std::net::TcpStream;

fn main() {
    let server_address = "127.0.0.1:2137";

    // Connect to the server
    match TcpStream::connect(server_address) {
        Ok(mut stream) => {
            println!("Successfully connected to server at {}", server_address);

            // Send a message to the server
            let message = "Hello, Server!";
            stream.write_all(message.as_bytes()).unwrap();
            println!("Sent message: {}", message);

            // Wait for a response from the server
            let mut buffer = [0; 512];
            match stream.read(&mut buffer) {
                Ok(size) => {
                    let response = String::from_utf8_lossy(&buffer[..size]);
                    println!("Received from server: {}", response);
                }
                Err(e) => {
                    eprintln!("Failed to receive data: {}", e);
                }
            }
        }
        Err(e) => {
            eprintln!("Failed to connect to server: {}", e);
        }
    }
}
