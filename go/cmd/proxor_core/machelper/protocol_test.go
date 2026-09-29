package machelper

import (
	"bufio"
	"bytes"
	"encoding/json"
	"errors"
	"strings"
	"testing"
)

func TestReadRequestLineCap(t *testing.T) {
	line := strings.Repeat("a", MaxLineBytes+1) + "\n"
	r := bufio.NewReaderSize(strings.NewReader(line), 4096)
	_, err := ReadRequest(r)
	if !errors.Is(err, ErrLineTooLong) {
		t.Fatalf("expected ErrLineTooLong, got %v", err)
	}
}

func TestReadRequestLineCapWithoutNewline(t *testing.T) {
	data := strings.Repeat("a", MaxLineBytes+10)
	r := bufio.NewReaderSize(strings.NewReader(data), 4096)
	_, err := ReadRequest(r)
	if !errors.Is(err, ErrLineTooLong) {
		t.Fatalf("expected ErrLineTooLong, got %v", err)
	}
}

func TestReadRequestInvalidJSON(t *testing.T) {
	r := bufio.NewReader(strings.NewReader("not json\n"))
	_, err := ReadRequest(r)
	if err == nil || errors.Is(err, ErrLineTooLong) {
		t.Fatalf("expected a JSON error, got %v", err)
	}
}

func TestReadRequestDecode(t *testing.T) {
	r := bufio.NewReader(strings.NewReader(`{"id":7,"cmd":"tun_start","config":"{}","socksPort":2080}` + "\n"))
	req, err := ReadRequest(r)
	if err != nil {
		t.Fatal(err)
	}
	want := Request{ID: 7, Cmd: "tun_start", Config: "{}", SocksPort: 2080}
	if req.ID != want.ID || req.Cmd != want.Cmd || req.Config != want.Config || req.SocksPort != want.SocksPort {
		t.Fatalf("got %+v want %+v", req, want)
	}
}

func TestReadRequestMaxLineAccepted(t *testing.T) {
	// A line of exactly MaxLineBytes (content, excluding the newline) is allowed.
	prefix := `{"id":1,"cmd":"hello","config":"`
	suffix := `"}`
	pad := strings.Repeat("a", MaxLineBytes-len(prefix)-len(suffix))
	r := bufio.NewReaderSize(strings.NewReader(prefix+pad+suffix+"\n"), 4096)
	req, err := ReadRequest(r)
	if err != nil {
		t.Fatal(err)
	}
	if len(req.Config) != len(pad) {
		t.Fatalf("config length %d, want %d", len(req.Config), len(pad))
	}
}

func TestReadRequestSequential(t *testing.T) {
	r := bufio.NewReader(strings.NewReader(`{"id":1,"cmd":"hello"}` + "\n" + `{"id":2,"cmd":"status"}` + "\n"))
	a, err := ReadRequest(r)
	if err != nil || a.ID != 1 {
		t.Fatalf("first: %+v %v", a, err)
	}
	b, err := ReadRequest(r)
	if err != nil || b.ID != 2 {
		t.Fatalf("second: %+v %v", b, err)
	}
}

func TestWriteMessage(t *testing.T) {
	var buf bytes.Buffer
	if err := WriteMessage(&buf, Response{ID: 1, OK: true, Protocol: 1}); err != nil {
		t.Fatal(err)
	}
	out := buf.String()
	if !strings.HasSuffix(out, "\n") || strings.Count(out, "\n") != 1 {
		t.Fatalf("expected exactly one trailing newline, got %q", out)
	}
	var back Response
	if err := json.Unmarshal([]byte(out), &back); err != nil {
		t.Fatal(err)
	}
	if back.ID != 1 || !back.OK || back.Protocol != 1 {
		t.Fatalf("round trip mismatch: %+v", back)
	}
}

type countingWriter struct {
	writes int
	buf    bytes.Buffer
}

func (c *countingWriter) Write(p []byte) (int, error) {
	c.writes++
	return c.buf.Write(p)
}

func TestWriteMessageSingleWrite(t *testing.T) {
	var w countingWriter
	if err := WriteMessage(&w, Event{Event: EventTunReady}); err != nil {
		t.Fatal(err)
	}
	if w.writes != 1 {
		t.Fatalf("expected a single Write call, got %d", w.writes)
	}
}

func TestConstants(t *testing.T) {
	if ProtocolVersion != 1 {
		t.Fatalf("ProtocolVersion = %d", ProtocolVersion)
	}
	if len(SocketPath) >= 104 {
		t.Fatalf("SocketPath is %d bytes, must be < 104 (sun_path)", len(SocketPath))
	}
	if MaxLineBytes != 1<<20 {
		t.Fatalf("MaxLineBytes = %d", MaxLineBytes)
	}
	for _, c := range []string{CmdHello, CmdStatus, CmdTunStart, CmdTunStop, CmdSysproxyApply, CmdSysproxyRestore, CmdUninstall} {
		if c == "" {
			t.Fatal("empty command constant")
		}
	}
}
