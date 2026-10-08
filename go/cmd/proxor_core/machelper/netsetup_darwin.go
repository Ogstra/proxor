//go:build darwin

package machelper

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"os/exec"
	"strings"
	"time"
)

const networksetupPath = "/usr/sbin/networksetup"

const (
	defaultNetworksetupTimeout = 10 * time.Second
	maxNetworksetupErrorLen    = 300
)

// NetworkSetup runs the macOS networksetup tool. As root (the helper) it can
// change proxy settings on macOS 26+, where non-root writes are refused.
//
// The binary is always /usr/sbin/networksetup and the arguments are an argv
// slice, never a shell string, so a service name or bypass entry cannot inject
// anything. The environment is fixed and minimal.
type NetworkSetup struct {
	Timeout time.Duration // default 10 s
}

var _ Commander = NetworkSetup{}

// Run returns stdout. A non-zero exit, a timeout, or a "** Error:" report (which
// networksetup prints even when it exits 0) is an error.
func (n NetworkSetup) Run(args ...string) (string, error) {
	timeout := n.Timeout
	if timeout <= 0 {
		timeout = defaultNetworksetupTimeout
	}
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()

	cmd := exec.CommandContext(ctx, networksetupPath, args...)
	cmd.Env = []string{"PATH=/usr/bin:/bin:/usr/sbin:/sbin", "LANG=C"}
	cmd.WaitDelay = time.Second // do not hang on inherited pipes after a kill
	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	runErr := cmd.Run()

	out := stdout.String()
	if errors.Is(ctx.Err(), context.DeadlineExceeded) {
		return out, fmt.Errorf("networksetup timed out after %s", timeout)
	}
	reported := strings.Contains(out, "** Error:") || strings.Contains(stderr.String(), "** Error:")
	if runErr == nil && !reported {
		return out, nil
	}
	msg := strings.TrimSpace(stderr.String())
	if msg == "" || (reported && !strings.Contains(msg, "** Error:")) {
		msg = strings.TrimSpace(out)
	}
	if msg == "" && runErr != nil {
		msg = runErr.Error()
	}
	if len(msg) > maxNetworksetupErrorLen {
		msg = msg[:maxNetworksetupErrorLen]
	}
	return out, errors.New(msg)
}
