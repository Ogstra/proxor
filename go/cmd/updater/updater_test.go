package main

import "testing"

func TestShouldUpdate(t *testing.T) {
	tests := []struct {
		name             string
		currentVersion   string
		latestVersion    string
		allowPreReleases bool
		wantShouldUpdate bool
	}{
		{
			name:             "patch update",
			currentVersion:   "1.6",
			latestVersion:    "1.6.1",
			allowPreReleases: false,
			wantShouldUpdate: true,
		},
		{
			name:             "pre-release blocked",
			currentVersion:   "1.5.4",
			latestVersion:    "1.6-beta",
			allowPreReleases: false,
			wantShouldUpdate: false,
		},
		{
			name:             "pre-release allowed",
			currentVersion:   "1.5.4",
			latestVersion:    "1.6-beta",
			allowPreReleases: true,
			wantShouldUpdate: true,
		},
		{
			name:             "stable is not older than prerelease equivalent",
			currentVersion:   "1.6",
			latestVersion:    "1.6-beta",
			allowPreReleases: false,
			wantShouldUpdate: false,
		},
		{
			name:             "stable still not older than prerelease equivalent when allowed",
			currentVersion:   "1.6",
			latestVersion:    "1.6-beta",
			allowPreReleases: true,
			wantShouldUpdate: false,
		},
		{
			name:             "current newer than latest stable",
			currentVersion:   "1.6.1",
			latestVersion:    "1.6",
			allowPreReleases: false,
			wantShouldUpdate: false,
		},
		{
			name:             "prerelease current upgrades to stable",
			currentVersion:   "1.6-beta",
			latestVersion:    "1.6",
			allowPreReleases: false,
			wantShouldUpdate: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			got, err := ShouldUpdate(tt.currentVersion, tt.latestVersion, tt.allowPreReleases)
			if err != nil {
				t.Fatalf("ShouldUpdate() error = %v", err)
			}
			if got != tt.wantShouldUpdate {
				t.Fatalf("ShouldUpdate() = %v, want %v", got, tt.wantShouldUpdate)
			}
		})
	}
}
