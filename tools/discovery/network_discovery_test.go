package main

import (
	"bufio"
	"encoding/json"
	"net"
	"os"
	"testing"
	"time"
)

// captureEvents runs fn with stdout redirected and returns the events it emitted.
func captureEvents(t *testing.T, fn func()) []event {
	t.Helper()
	r, w, err := os.Pipe()
	if err != nil {
		t.Fatal(err)
	}
	saved := os.Stdout
	os.Stdout = w
	fn()
	os.Stdout = saved
	w.Close()

	var events []event
	sc := bufio.NewScanner(r)
	for sc.Scan() {
		var e event
		if err := json.Unmarshal(sc.Bytes(), &e); err != nil {
			t.Fatalf("not an event: %q", sc.Text())
		}
		events = append(events, e)
	}
	return events
}

func resetDiscovered() {
	mu.Lock()
	discovered = map[string]*trackedPort{}
	mu.Unlock()
}

func TestHandleResponseRelabelsOnRoleChange(t *testing.T) {
	resetDiscovered()
	const ip = "10.0.0.32"
	const uid = "393939383432510300390029"

	events := captureEvents(t, func() {
		handleResponse(ip, []byte("STM32H743_"+uid+"_CUSAPP_0.1.3_1.0.0"))
		handleResponse(ip, []byte("STM32H743_"+uid+"_CUSAPP_0.1.3_1.0.0")) // unchanged: no event
		handleResponse(ip, []byte("STM32H743_"+uid+"_BOOTLD-INVALID_0.1.3_-"))
	})

	if len(events) != 3 {
		t.Fatalf("want add, remove, add; got %d events: %+v", len(events), events)
	}
	want := []struct{ typ, label, server string }{
		{"add", "STM32H743 (0.1.3_1.0.0) @ " + ip, "CUSAPP"},
		{"remove", "STM32H743 (0.1.3_1.0.0) @ " + ip, "CUSAPP"},
		{"add", "STM32H743 (0.1.3_-) @ " + ip, "BOOTLD-INVALID"},
	}
	for i, w := range want {
		e := events[i]
		if e.EventType != w.typ || e.Port == nil || e.Port.Label != w.label ||
			e.Port.Properties["serverName"] != w.server {
			t.Errorf("event %d: want %s %q (%s), got %+v", i, w.typ, w.label, w.server, e)
		}
	}
	if ports := listPorts(); len(ports) != 1 || ports[0].Properties["serverName"] != "BOOTLD-INVALID" {
		t.Errorf("LIST should hold only the new identity, got %+v", ports)
	}
}

func TestReadLoopDropsFailedSocket(t *testing.T) {
	conn, err := net.ListenUDP("udp4", &net.UDPAddr{IP: net.IPv4(127, 0, 0, 1)})
	if err != nil {
		t.Fatal(err)
	}
	ic := ifaceConn{conn: conn, bcast: net.IPv4(127, 0, 0, 1), name: "test"}
	connsMu.Lock()
	activeConns = map[string]ifaceConn{"test": ic}
	connsMu.Unlock()

	done := make(chan struct{})
	go func() {
		readLoop("test", ic, make(chan struct{}))
		close(done)
	}()
	conn.Close() // a non-timeout error from now on

	select {
	case <-done:
	case <-time.After(3 * time.Second):
		t.Fatal("readLoop kept running on a failed socket")
	}
	connsMu.Lock()
	defer connsMu.Unlock()
	if _, ok := activeConns["test"]; ok {
		t.Error("failed socket still in activeConns, rescan would never reopen it")
	}
}
