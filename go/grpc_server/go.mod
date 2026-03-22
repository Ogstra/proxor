module grpc_server

go 1.26.1

require (
	github.com/grpc-ecosystem/go-grpc-middleware/v2 v2.3.3
	github.com/matsuridayo/libneko v1.0.0 // replaced
	google.golang.org/grpc v1.79.3
	google.golang.org/protobuf v1.36.11
)

require (
	golang.org/x/net v0.52.0 // indirect
	golang.org/x/sys v0.42.0 // indirect
	golang.org/x/text v0.35.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20251202230838-ff82c1b0f217 // indirect
)

replace github.com/matsuridayo/libneko v1.0.0 => ../../../libneko
