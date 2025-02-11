use std::ptr;

use windows::{
    Win32::Foundation::*,
    Win32::UI::WindowsAndMessaging::*,
    Win32::System::Threading::*,
    Win32::System::ProcessStatus::*
};

pub fn get_foreground_window_hwnd() -> Option<HWND> {
    unsafe {
        let hwnd = GetForegroundWindow();
        if hwnd == HWND(ptr::null_mut()) { return None; }
        return Some(hwnd);
    }
}

pub fn get_file_path_hwnd(hwnd: HWND) -> Option<String> {
    unsafe {
        let mut pid = 0;
        GetWindowThreadProcessId(hwnd, Some(&mut pid));
        if pid == 0 { return None; }

        let p_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid)
            .expect("Failed to open process");

        // Buffer to store the file path
        let mut buffer: [u16; MAX_PATH as usize] = [0; MAX_PATH as usize];
        let len = GetProcessImageFileNameW(p_handle, &mut buffer) as usize;

        // Close the process handle
        let _ = CloseHandle(p_handle)
            .expect("Failed to close process");

        // Convert the buffer to a string
        let file_path = String::from_utf16_lossy(&buffer[..len]);

        // process the file path
        let file_path = file_path.replace("\\", "/");

        return Some(file_path);
    }
}

pub fn is_game(hwnd: Option<HWND>) -> bool {

    let handle = match hwnd {
        Some(hwnd) => hwnd,
        None => return false 
    };

    let file_path = match get_file_path_hwnd(handle){
        Some(fp) => fp,
        None => return false 
    };

    if file_path.contains("steamapps") {
        return true;
    }

   false 
}

// Check if the window is alive
// WARNING: This function is not reliable, the function ISWindow checks if 
// a window exists with the given handle, however, windows sometimes reuses
// handles, so it is possible that the handle is valid but the window is not.
pub fn is_hwnd_alive(hwnd: Option<HWND>) -> bool {
    unsafe{
        let is_window: BOOL = IsWindow(hwnd);
        return is_window == BOOL::from(true);
    }
}

pub fn hwnd_to_string(hwnd: HWND) -> Option<String> {
    unsafe {
        let mut title: [u8; 64] = [0; 64];
        let complete = GetWindowTextA(hwnd, &mut title);
        let title = title.iter().position(|&x| x == 0)
            .map_or(&title[..], |i| &title[..i]);
    
        let title_str = String::from_utf8_lossy(title).to_string();
        if title_str.is_empty() { return None; }

        return Some(title_str);
    }
}
