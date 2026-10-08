//go:build !darwin

package machelper

import (
	"fmt"
	"os"
)

// Main is the `proxor_core helper` entry point. The privileged helper only
// exists on macOS; every other platform refuses.
func Main(args []string) int {
	fmt.Fprintln(os.Stderr, "proxor_core helper is only available on macOS")
	return 1
}
