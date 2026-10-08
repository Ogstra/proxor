package main

import (
	"os/exec"
	"syscall"
	"unsafe"
)

// MessageBoxPlain of Win32 API.
func MessageBoxPlain(title, caption string) int {
	const (
		NULL  = 0
		MB_OK = 0
	)
	return MessageBox(NULL, caption, title, MB_OK)
}

// MessageBoxYesNo shows a warning with Yes/No buttons and reports whether Yes was chosen.
func MessageBoxYesNo(title, caption string) bool {
	const (
		NULL           = 0
		MB_YESNO       = 0x4
		MB_ICONWARNING = 0x30
		IDYES          = 6
	)
	return MessageBox(NULL, caption, title, MB_YESNO|MB_ICONWARNING) == IDYES
}

// openURL opens a URL in the default browser.
func openURL(url string) {
	_ = exec.Command("rundll32", "url.dll,FileProtocolHandler", url).Start()
}

// MessageBox of Win32 API.
func MessageBox(hwnd uintptr, caption, title string, flags uint) int {
	ret, _, _ := syscall.NewLazyDLL("user32.dll").NewProc("MessageBoxW").Call(
		uintptr(hwnd),
		uintptr(unsafe.Pointer(syscall.StringToUTF16Ptr(caption))),
		uintptr(unsafe.Pointer(syscall.StringToUTF16Ptr(title))),
		uintptr(flags))

	return int(ret)
}
