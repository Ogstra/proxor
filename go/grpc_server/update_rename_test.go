package grpc_server

import (
	"errors"
	"testing"
)

func TestRenameWithRetryRecoversFromTransientLock(t *testing.T) {
	calls := 0
	old := renameFile
	renameFile = func(from, to string) error {
		calls++
		if calls < 3 {
			return errors.New("The process cannot access the file because it is being used by another process.")
		}
		return nil
	}
	defer func() { renameFile = old }()

	if err := renameWithRetry("a.part", "a.zip"); err != nil {
		t.Fatalf("expected success after retries, got %v", err)
	}
	if calls != 3 {
		t.Fatalf("expected 3 attempts, got %d", calls)
	}
}

func TestRenameWithRetryGivesUpWithTheLastError(t *testing.T) {
	stubRetrySleep(t)
	want := errors.New("still locked")
	old := renameFile
	renameFile = func(from, to string) error { return want }
	calls := 0
	inner := renameFile
	renameFile = func(from, to string) error { calls++; return inner(from, to) }
	defer func() { renameFile = old }()

	if err := renameWithRetry("a.part", "a.zip"); !errors.Is(err, want) {
		t.Fatalf("expected the last error, got %v", err)
	}
	if calls != renameAttempts {
		t.Fatalf("expected %d attempts, got %d", renameAttempts, calls)
	}
}
