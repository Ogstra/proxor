package boxmain

import (
	"bytes"
	"io"
	"testing"
)

type nilBuffer struct {
	bytes.Buffer
}

func TestSetLogWriterIgnoresTypedNil(t *testing.T) {
	var writer *nilBuffer
	SetLogWriter(writer)
	if platformWriter != nil {
		t.Fatalf("expected platform writer to stay nil for typed nil writer")
	}
}

func TestSetLogWriterWritesWithConcreteWriter(t *testing.T) {
	var buffer bytes.Buffer
	SetLogWriter(&buffer)
	writer, ok := platformWriter.(stdPlatformWriter)
	if !ok {
		t.Fatalf("expected stdPlatformWriter, got %T", platformWriter)
	}
	writer.WriteMessage(0, "hello")
	if got := buffer.String(); got != "hello\n" {
		t.Fatalf("unexpected log output: %q", got)
	}
}

func TestStdPlatformWriterSkipsTypedNil(t *testing.T) {
	var writer *nilBuffer
	stdPlatformWriter{writer: io.Writer(writer)}.WriteMessage(0, "ignored")
}
