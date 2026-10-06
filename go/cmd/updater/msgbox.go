//go:build !windows

package main

func MessageBoxPlain(title, caption string) int {
	return 0
}

func MessageBoxYesNo(title, caption string) bool { return false }

func openURL(url string) {}
