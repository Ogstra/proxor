package main

import (
	"context"
	"os"
	"path/filepath"
	"slices"
	"strings"
	"testing"

	"grpc_server/gen"
)

func TestPhase10ValidateMatrix(t *testing.T) {
	entries, err := os.ReadDir(filepath.Join("testdata", "phase10"))
	if err != nil {
		t.Fatalf("read fixtures: %v", err)
	}

	var names []string
	for _, entry := range entries {
		if entry.IsDir() || filepath.Ext(entry.Name()) != ".json" {
			continue
		}
		names = append(names, entry.Name())
	}
	slices.Sort(names)

	if len(names) != 9 {
		t.Fatalf("expected 9 fixtures, got %d", len(names))
	}

	srv := &server{}
	for _, name := range names {
		name := name
		t.Run(strings.TrimSuffix(name, ".json"), func(t *testing.T) {
			payload, err := os.ReadFile(filepath.Join("testdata", "phase10", name))
			if err != nil {
				t.Fatalf("read fixture %s: %v", name, err)
			}

			resp, err := srv.Validate(context.Background(), &gen.LoadConfigReq{CoreConfig: string(payload)})
			if err != nil {
				t.Fatalf("validate %s: %v", name, err)
			}
			if got := resp.GetError(); got != "" {
				if (name == "hysteria2.json" || name == "tuic.json") && strings.Contains(got, "QUIC is not included in this build") {
					t.Skipf("fixture %s requires with_quic tags: %s", name, got)
				}
				t.Fatalf("validate %s returned error: %s", name, got)
			}
		})
	}
}
