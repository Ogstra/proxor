package grpc_server

import (
	"context"
	"net"
	"testing"
	"time"

	"grpc_server/auth"
	"grpc_server/gen"

	grpc_auth "github.com/grpc-ecosystem/go-grpc-middleware/v2/interceptors/auth"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
)

type integrationTestServer struct {
	gen.UnimplementedLibcoreServiceServer
}

func (integrationTestServer) Update(context.Context, *gen.UpdateReq) (*gen.UpdateResp, error) {
	return &gen.UpdateResp{}, nil
}

func TestRunCoreInterceptorsRejectBadToken(t *testing.T) {
	lis, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("listen: %v", err)
	}

	auther := auth.Authenticator{Token: "expected-token"}
	srv := grpc.NewServer(
		grpc.StreamInterceptor(grpc_auth.StreamServerInterceptor(auther.Authenticate)),
		grpc.UnaryInterceptor(grpc_auth.UnaryServerInterceptor(auther.Authenticate)),
	)
	gen.RegisterLibcoreServiceServer(srv, integrationTestServer{})

	go func() {
		_ = srv.Serve(lis)
	}()
	t.Cleanup(func() {
		srv.Stop()
	})

	conn, err := grpc.NewClient(
		"passthrough:///"+lis.Addr().String(),
		grpc.WithTransportCredentials(insecure.NewCredentials()),
	)
	if err != nil {
		t.Fatalf("new client: %v", err)
	}
	defer conn.Close()

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	ctx = metadata.AppendToOutgoingContext(ctx, "nekoray_auth", "wrong-token")

	client := gen.NewLibcoreServiceClient(conn)
	_, err = client.Update(ctx, &gen.UpdateReq{})
	if status.Code(err) != codes.Unauthenticated {
		t.Fatalf("expected Unauthenticated, got %v (err=%v)", status.Code(err), err)
	}
}
