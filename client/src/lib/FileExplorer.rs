use std::{fs, path::PathBuf};

pub fn get_dir_contents(src_dir: &str) -> Vec<String> {
    let path = fs::read_dir(src_dir).unwrap();

    let mut files: Vec<String> = vec![];

    for file in path {
        let path_name = file.unwrap().path();
        let file: String = path_name.into_os_string().into_string().unwrap();
        files.push(file);
    }

    files
}
