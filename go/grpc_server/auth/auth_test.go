package auth

import (
	"context"
	"testing"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

func TestAuthenticateMissingMetadata(t *testing.T) {
	auther := Authenticator{Token: "secret"}

	_, err := auther.Authenticate(context.Background())
	if status.Code(err) != codes.Unauthenticated {
		t.Fatalf("expected Unauthenticated, got %v", status.Code(err))
	}
}

func TestAuthenticateWrongToken(t *testing.T) {
	auther := Authenticator{Token: "secret"}
	ctx := metadata.NewIncomingContext(context.Background(), metadata.Pairs("nekoray_auth", "wrong"))

	_, err := auther.Authenticate(ctx)
	if status.Code(err) != codes.Unauthenticated {
		t.Fatalf("expected Unauthenticated, got %v", status.Code(err))
	}
}

func TestAuthenticateMultipleHeaderValues(t *testing.T) {
	auther := Authenticator{Token: "secret"}
	ctx := metadata.NewIncomingContext(context.Background(), metadata.MD{
		"nekoray_auth": []string{"one", "two"},
	})

	_, err := auther.Authenticate(ctx)
	if status.Code(err) != codes.Unauthenticated {
		t.Fatalf("expected Unauthenticated, got %v", status.Code(err))
	}
}

func TestAuthenticatePurgesHeaderOnSuccess(t *testing.T) {
	auther := Authenticator{Token: "secret"}
	ctx := metadata.NewIncomingContext(context.Background(), metadata.Pairs(
		"nekoray_auth", "secret",
		"other", "keep",
	))

	newCtx, err := auther.Authenticate(ctx)
	if err != nil {
		t.Fatalf("expected nil error, got %v", err)
	}

	md, ok := metadata.FromIncomingContext(newCtx)
	if !ok {
		t.Fatal("expected metadata in returned context")
	}
	if vals := md.Get("nekoray_auth"); len(vals) != 0 {
		t.Fatalf("expected nekoray_auth to be purged, got %v", vals)
	}
	if vals := md.Get("other"); len(vals) != 1 || vals[0] != "keep" {
		t.Fatalf("expected other header to be preserved, got %v", vals)
	}
}
