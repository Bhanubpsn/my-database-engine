package main

import (
	"bufio"
	"context"
	"fmt"
	"log"
	"os"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	pb "github.com/bhanubpsn/bpsndb/proto"
)

type Replicator struct {
	primary     pb.DatabaseEngineClient
	secondaries []pb.DatabaseEngineClient
	roundRobin  int
}

func isWriteCommand(cmd string) bool {
	if cmd[0] == '.' {
		return true
	}
	lowered := string(cmd[0:6])
	return lowered == "insert" || lowered == "delete" || lowered == "update"
}

func (r *Replicator) HandleCommand(cmd string) (*pb.CommandResponse, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 500*time.Millisecond)
	defer cancel()

	if isWriteCommand(cmd) {
		// Writing in Primary
		resp, err := r.primary.Execute(ctx, &pb.CommandRequest{Statement: cmd})
		if err != nil || resp.Status != pb.CommandResponse_SUCCESS {
			return resp, err
		}

		// Broadcast to Secondaries (Async)
		for _, s := range r.secondaries {
			go s.Execute(context.Background(), &pb.CommandRequest{Statement: cmd})
		}
		return resp, nil

	} else {
		if len(r.secondaries) == 0 {
			return r.primary.Execute(ctx, &pb.CommandRequest{Statement: cmd})
		}
		target := r.secondaries[r.roundRobin%len(r.secondaries)]
		r.roundRobin++
		return target.Execute(ctx, &pb.CommandRequest{Statement: cmd})
	}
}

func main() {
	addresses := []string{"localhost:50051", "localhost:50052", "localhost:50053"}
	clients := make([]pb.DatabaseEngineClient, len(addresses))

	for i, addr := range addresses {
		// Using NewClient in newer gRPC versions
		conn, err := grpc.NewClient(addr, grpc.WithTransportCredentials(insecure.NewCredentials()))
		if err != nil {
			log.Fatalf("failed to connect to %s: %v", addr, err)
		}
		clients[i] = pb.NewDatabaseEngineClient(conn)
	}

	replicator := &Replicator{
		primary:     clients[0],
		secondaries: clients[1:],
	}

	scanner := bufio.NewScanner(os.Stdin)
	fmt.Println("BPSNDB Distributed Console")
	fmt.Print("BpsnDB >> ")

	for scanner.Scan() {
		input := scanner.Text()

		resp, err := replicator.HandleCommand(input)
		if err != nil {
			fmt.Printf("RPC Error: %v\n", err)
		} else {
			fmt.Printf("[%s] %s\n", resp.Status, resp.Message)
			for _, r := range resp.Rows {
				fmt.Printf("  -> %d, %s, %s\n", r.Id, r.Username, r.Email)
			}
		}
		if input == ".exit" {
			break
		}
		fmt.Print("BpsnDB >> ")
	}
}
