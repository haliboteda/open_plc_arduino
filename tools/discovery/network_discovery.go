package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"sync"
	"time"
)

// Protocol constants must match the firmware side:
// cores/arduino/stm32/IAP_config.h (OPENPLC_SERVER_PORT, OPENPLC_DEVICE_NAME,
// UDP_SERVER_NAME, OPENPLC_CUSAPP_VERSION) and
// libraries/OpenPLC_IAP/src/udp_server.c (udp_server_recv).
const (
	discoveryPort     = 56865
	broadcastMessage  = "openplc_server_where_r_y"
	broadcastInterval = 30 * time.Second

	// Shared with IAPTool (IAPTranfer_Tool/uploadlock.go) -- both sides must
	// agree on the name and on how long a leftover lock stays believable.
	uploadLockName   = "openplc-iap-upload.lock"
	uploadLockMaxAge = 90 * time.Second
	rescanInterval   = 5 * time.Second
	staleSweepPeriod = 10 * time.Second
	staleTimeout     = 90 * time.Second // ~3 missed broadcast cycles
	readTimeout      = 500 * time.Millisecond
)

type portInfo struct {
	Address       string            `json:"address"`
	Label         string            `json:"label"`
	Protocol      string            `json:"protocol"`
	ProtocolLabel string            `json:"protocolLabel"`
	HardwareID    string            `json:"hardwareId,omitempty"`
	Properties    map[string]string `json:"properties"`
}

type event struct {
	EventType       string      `json:"eventType"`
	Message         string      `json:"message,omitempty"`
	ProtocolVersion int         `json:"protocolVersion,omitempty"`
	Port            *portInfo   `json:"port,omitempty"`
	Ports           []*portInfo `json:"ports"`
}

type trackedPort struct {
	info     *portInfo
	uid      string
	lastSeen time.Time
}

var (
	mu         sync.Mutex
	emitMu     sync.Mutex
	discovered = map[string]*trackedPort{} // keyed by IP
	stopChan   chan struct{}
	active     bool
	logFile    *os.File
)

func logf(format string, args ...interface{}) {
	if logFile != nil {
		t := time.Now().Format("15:04:05.000")
		fmt.Fprintf(logFile, "[%s] %s\n", t, fmt.Sprintf(format, args...))
	}
}

func emit(e event) {
	if e.Ports == nil {
		e.Ports = []*portInfo{}
	}
	data, err := json.Marshal(e)
	if err != nil {
		logf("ERROR marshal: %v", err)
		return
	}
	emitMu.Lock()
	defer emitMu.Unlock()
	n, werr := fmt.Println(string(data))
	logf("EMIT(%d bytes, err=%v): %s", n, werr, string(data))
}

// isRealHardwareInterface, isPhysicalInterface's authoritative half, is
// implemented per-OS (iface_linux.go / iface_darwin.go / iface_windows.go)
// since a network adapter's friendly Name is not a reliable signal on any
// platform (Windows in particular renumbers virtual adapters into generic
// names like "Ethernet 5", indistinguishable by name from a real NIC).

func isPhysicalInterface(iface net.Interface) bool {
	if iface.Flags&net.FlagUp == 0 {
		return false
	}
	if iface.Flags&net.FlagLoopback != 0 {
		return false
	}
	if iface.Flags&net.FlagPointToPoint != 0 {
		return false
	}
	if len(iface.HardwareAddr) == 0 {
		return false
	}
	return isRealHardwareInterface(iface)
}

// hwCache maps interface name -> "is this backed by real hardware", as
// classified by the OS-specific classifyHardware. It's refreshed only when
// the set of interfaces changes, or at most every hwCacheMaxAge, since the
// classifiers can shell out to an external command (PowerShell, networksetup).
var (
	hwCacheMu  sync.Mutex
	hwCache    map[string]bool
	hwCacheKey string
	hwCacheAt  time.Time
)

const hwCacheMaxAge = 60 * time.Second

func refreshHardwareCacheIfNeeded(ifaces []net.Interface) {
	names := make([]string, 0, len(ifaces))
	for _, ifc := range ifaces {
		names = append(names, ifc.Name)
	}
	sort.Strings(names)
	key := strings.Join(names, ",")

	hwCacheMu.Lock()
	stale := hwCache == nil || key != hwCacheKey || time.Since(hwCacheAt) > hwCacheMaxAge
	hwCacheMu.Unlock()
	if !stale {
		return
	}

	fresh := classifyHardware(ifaces) // OS-specific; nil means "unavailable"
	logf("hardware classification refreshed: %+v", fresh)

	hwCacheMu.Lock()
	defer hwCacheMu.Unlock()
	if fresh != nil {
		hwCache = fresh
	} else if hwCache == nil {
		hwCache = map[string]bool{}
	}
	hwCacheKey = key
	hwCacheAt = time.Now()
}

// isRealHardwareInterface fails open (treats an interface as real) whenever
// the OS-specific classifier is unavailable or doesn't know about a given
// interface, so a classifier bug never silently hides every NIC.
func isRealHardwareInterface(iface net.Interface) bool {
	hwCacheMu.Lock()
	defer hwCacheMu.Unlock()
	if hwCache == nil {
		return true
	}
	v, ok := hwCache[iface.Name]
	if !ok {
		return true
	}
	return v
}

func subnetBroadcast(ipNet *net.IPNet) net.IP {
	ip4 := ipNet.IP.To4()
	if ip4 == nil {
		return nil
	}
	bcast := make(net.IP, 4)
	for i := 0; i < 4; i++ {
		bcast[i] = ip4[i] | ^ipNet.Mask[i]
	}
	return bcast
}

type ifaceConn struct {
	conn  *net.UDPConn
	bcast net.IP
	name  string
}

func openPhysicalConns() []ifaceConn {
	var result []ifaceConn
	ifaces, err := net.Interfaces()
	if err != nil {
		return result
	}
	refreshHardwareCacheIfNeeded(ifaces)
	for _, iface := range ifaces {
		if !isPhysicalInterface(iface) {
			continue
		}
		addrs, err := iface.Addrs()
		if err != nil {
			continue
		}
		for _, addr := range addrs {
			ipNet, ok := addr.(*net.IPNet)
			if !ok {
				continue
			}
			ip4 := ipNet.IP.To4()
			if ip4 == nil {
				continue
			}
			bcast := subnetBroadcast(ipNet)
			if bcast == nil {
				continue
			}
			conn, err := net.ListenUDP("udp4", &net.UDPAddr{IP: ip4, Port: 0})
			if err != nil {
				continue
			}
			result = append(result, ifaceConn{conn: conn, bcast: bcast, name: iface.Name})
		}
	}
	return result
}

// handleResponse parses a reply from the firmware, which has the form
// "<deviceName>_<uidHex>_<serverName>_<version>" (see udp_server_recv in
// libraries/OpenPLC_IAP/src/udp_server.c).
func handleResponse(ip string, payload []byte) {
	logf("UDP from=%s payload=%q", ip, payload)

	raw := strings.TrimSpace(string(payload))
	parts := strings.SplitN(raw, "_", 4)
	deviceName, uid, serverName, version := "OpenPLC", "", "", ""
	if len(parts) == 4 {
		deviceName, uid, serverName, version = parts[0], parts[1], parts[2], parts[3]
	} else if len(parts) > 0 && parts[0] != "" {
		deviceName = parts[0]
	}

	mu.Lock()
	defer mu.Unlock()

	now := time.Now()
	if existing, ok := discovered[ip]; ok {
		if existing.uid == uid {
			existing.lastSeen = now
			return
		}
		// Same IP, different device UID (DHCP lease reassigned) - swap it out.
		logf("IP %s now reports a different uid (%q -> %q), replacing", ip, existing.uid, uid)
		emit(event{EventType: "remove", Port: existing.info})
		delete(discovered, ip)
	}

	label := deviceName + " @ " + ip
	if version != "" {
		label = fmt.Sprintf("%s (%s) @ %s", deviceName, version, ip)
	}
	p := &portInfo{
		Address:       ip,
		Label:         label,
		Protocol:      "network",
		ProtocolLabel: "Network Port",
		HardwareID:    uid,
		Properties: map[string]string{
			"deviceName": deviceName,
			"serverName": serverName,
			"version":    version,
			"port":       fmt.Sprintf("%d", discoveryPort),
		},
	}
	discovered[ip] = &trackedPort{info: p, uid: uid, lastSeen: now}
	logf("NEW port: %s (%s), emitting add", ip, uid)
	emit(event{EventType: "add", Port: p})
}

func sweepStale() {
	mu.Lock()
	defer mu.Unlock()
	cutoff := time.Now().Add(-staleTimeout)
	for ip, tp := range discovered {
		if tp.lastSeen.Before(cutoff) {
			logf("STALE port: %s (last seen %s ago), emitting remove", ip, time.Since(tp.lastSeen))
			emit(event{EventType: "remove", Port: tp.info})
			delete(discovered, ip)
		}
	}
}

var (
	connsMu     sync.Mutex
	activeConns = map[string]ifaceConn{} // keyed by local IP
)

func launchConn(ic ifaceConn) {
	logf("startDiscovery: broadcasting via %s -> %s", ic.name, ic.bcast)
	fmt.Fprintf(os.Stderr, "[network_discovery] broadcasting via %s -> %s\n", ic.name, ic.bcast)
	go func() {
		defer ic.conn.Close()
		buf := make([]byte, 1024)
		for {
			select {
			case <-stopChan:
				return
			default:
			}
			ic.conn.SetReadDeadline(time.Now().Add(readTimeout))
			n, addr, err := ic.conn.ReadFromUDP(buf)
			if err != nil {
				continue
			}
			handleResponse(addr.IP.String(), buf[:n])
		}
	}()
}

func syncPhysicalConns() {
	newConns := openPhysicalConns()
	connsMu.Lock()
	defer connsMu.Unlock()
	for _, ic := range newConns {
		localIP := ic.conn.LocalAddr().(*net.UDPAddr).IP.String()
		if _, exists := activeConns[localIP]; exists {
			ic.conn.Close()
			continue
		}
		activeConns[localIP] = ic
		launchConn(ic)
	}
}

// The board rate-limits discovery replies per source IP, so a broadcast sent
// while IAPTool is flashing from this same host can consume the budget and make
// the flashing tool's own query go unanswered. Standing aside costs one skipped
// refresh; not standing aside costs a failed upload.
func uploadInProgress() bool {
	info, err := os.Stat(filepath.Join(os.TempDir(), uploadLockName))
	if err != nil {
		return false
	}
	// A lock left behind by a crashed upload must not silence discovery forever.
	if time.Since(info.ModTime()) > uploadLockMaxAge {
		return false
	}
	return true
}

func doBroadcast() {
	if uploadInProgress() {
		logf("broadcast skipped: an upload is in progress on this host")
		return
	}

	connsMu.Lock()
	defer connsMu.Unlock()
	for _, ic := range activeConns {
		_, err := ic.conn.WriteToUDP([]byte(broadcastMessage), &net.UDPAddr{IP: ic.bcast, Port: discoveryPort})
		logf("broadcast via %s -> %s: err=%v", ic.name, ic.bcast, err)
	}
}

func startDiscovery() {
	if active {
		logf("startDiscovery: already active")
		return
	}
	stopChan = make(chan struct{})
	active = true
	activeConns = map[string]ifaceConn{}

	syncPhysicalConns()

	if len(activeConns) == 0 {
		conn, err := net.ListenUDP("udp4", &net.UDPAddr{Port: 0})
		if err != nil {
			logf("startDiscovery: fallback UDP listen failed: %v", err)
			return
		}
		ic := ifaceConn{conn: conn, bcast: net.IPv4bcast, name: "fallback"}
		activeConns["fallback"] = ic
		launchConn(ic)
		logf("startDiscovery: no physical NICs, using fallback broadcast")
		fmt.Fprintln(os.Stderr, "[network_discovery] no physical NICs found, using fallback broadcast")
	}

	go func() {
		doBroadcast()
		broadcastTicker := time.NewTicker(broadcastInterval)
		rescanTicker := time.NewTicker(rescanInterval)
		staleTicker := time.NewTicker(staleSweepPeriod)
		defer broadcastTicker.Stop()
		defer rescanTicker.Stop()
		defer staleTicker.Stop()
		for {
			select {
			case <-stopChan:
				return
			case <-rescanTicker.C:
				syncPhysicalConns()
			case <-broadcastTicker.C:
				doBroadcast()
			case <-staleTicker.C:
				sweepStale()
			}
		}
	}()
}

func stopDiscovery() {
	if !active {
		logf("stopDiscovery: not active")
		return
	}
	close(stopChan)
	active = false

	connsMu.Lock()
	for _, ic := range activeConns {
		ic.conn.Close()
	}
	activeConns = map[string]ifaceConn{}
	connsMu.Unlock()

	mu.Lock()
	defer mu.Unlock()
	for ip, tp := range discovered {
		logf("removing port: %s", ip)
		emit(event{EventType: "remove", Port: tp.info})
		delete(discovered, ip)
	}
}

func listPorts() []*portInfo {
	mu.Lock()
	defer mu.Unlock()
	ports := make([]*portInfo, 0, len(discovered))
	for _, tp := range discovered {
		ports = append(ports, tp.info)
	}
	return ports
}

func main() {
	logFile, _ = os.OpenFile(filepath.Join(os.TempDir(), "network_discovery.log"), os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0644)
	if logFile != nil {
		logf("=== network_discovery started ===")
		defer logFile.Close()
	}

	scanner := bufio.NewScanner(os.Stdin)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		logf("STDIN: %q", line)
		parts := strings.Fields(line)
		if len(parts) == 0 {
			continue
		}
		switch parts[0] {
		case "HELLO":
			emit(event{EventType: "hello", ProtocolVersion: 1, Message: "OK"})
		case "START":
			startDiscovery()
			emit(event{EventType: "start", Message: "OK"})
		case "START_SYNC":
			startDiscovery()
			emit(event{EventType: "start_sync", Message: "OK"})
		case "STOP":
			stopDiscovery()
			emit(event{EventType: "stop", Message: "OK"})
		case "QUIT":
			stopDiscovery()
			emit(event{EventType: "quit", Message: "OK"})
			return
		case "LIST":
			emit(event{EventType: "list", Ports: listPorts()})
		default:
			logf("UNKNOWN command: %q", parts[0])
		}
	}
	logf("stdin EOF, exiting")
}
