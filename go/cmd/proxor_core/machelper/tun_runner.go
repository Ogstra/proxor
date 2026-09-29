package machelper

import (
	"context"
	"fmt"
	"io"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"
	"unicode/utf8"

	"github.com/sagernet/sing-box/option"

	"proxor_core/boxmain"
)

const (
	defaultSocksWait = 30 * time.Second
	defaultSocksPoll = 250 * time.Millisecond
	socksDialTimeout = time.Second
	// stopWaitBound caps how long Stop waits for a start goroutine that is
	// already inside create; create honours the cancelled context, so this is
	// only a safety net.
	stopWaitBound = 10 * time.Second
	maxLogLine    = 4 << 10
)

// tunHooks are the OS-facing steps. Production values wrap boxmain and net;
// tests inject fakes so the lifecycle is proven without a utun or root.
type tunHooks struct {
	check        func(ctx context.Context, o option.Options) error
	create       func(ctx context.Context, o option.Options) (io.Closer, context.CancelFunc, error)
	dial         func(addr string, timeout time.Duration) error
	setLogWriter func(io.Writer)
}

func productionTunHooks() tunHooks {
	return tunHooks{
		check: boxmain.CheckOptions,
		create: func(ctx context.Context, o option.Options) (io.Closer, context.CancelFunc, error) {
			instance, cancel, err := boxmain.CreateFromOptions(ctx, o)
			if err != nil {
				return nil, nil, err
			}
			return instance, cancel, nil
		},
		dial: func(addr string, timeout time.Duration) error {
			conn, err := net.DialTimeout("tcp", addr, timeout)
			if err != nil {
				return err
			}
			return conn.Close()
		},
		setLogWriter: boxmain.SetLogWriter,
	}
}

// BoxTunRunner runs the Tun sing-box instance in the helper process (as root).
// It only ever runs options that came out of ValidateTunConfig.
//
// Start validates and checks synchronously (an error means nothing was
// started), then a goroutine waits for the user core's SOCKS port, starts the
// instance and reports tun_ready / tun_stopped / log through emit. Stop is
// idempotent, never emits and never calls back into the server, so the server
// may call it while holding its own lock. Start may call emit synchronously
// from its goroutine at any time.
type BoxTunRunner struct {
	// SocksWait bounds the wait for the core's SOCKS port (default 30 s).
	SocksWait time.Duration

	hooks tunHooks
	poll  time.Duration

	mu       sync.Mutex
	gen      uint64             // bumped by every Start and Stop; events of an older generation are dropped
	active   bool               // waiting for the core, or the instance is up
	stopWait context.CancelFunc // cancels the wait and the run context of the current start
	closer   io.Closer          // the running instance
	cancel   context.CancelFunc // the running instance's cancel func
	done     chan struct{}      // closed when the current start goroutine has returned
}

var _ TunRunner = (*BoxTunRunner)(nil)

func NewBoxTunRunner() *BoxTunRunner {
	return &BoxTunRunner{SocksWait: defaultSocksWait, hooks: productionTunHooks(), poll: defaultSocksPoll}
}

// Running reports whether a Tun instance is up or being brought up, so the
// server's cleanup paths also stop an instance that is still waiting for SOCKS.
func (r *BoxTunRunner) Running() bool {
	r.mu.Lock()
	defer r.mu.Unlock()
	return r.active
}

func (r *BoxTunRunner) Start(config []byte, socksPort int, emit func(Event)) error {
	if err := r.Stop(); err != nil {
		return fmt.Errorf("stop previous tun: %w", err)
	}
	ctx, opts, err := ValidateTunConfig(config, socksPort)
	if err != nil {
		return err
	}
	if err := r.hooks.check(ctx, opts); err != nil {
		return fmt.Errorf("check config: %w", err)
	}
	if emit == nil {
		emit = func(Event) {}
	}
	runCtx, stopWait := context.WithCancel(ctx)
	done := make(chan struct{})
	r.mu.Lock()
	r.gen++
	gen := r.gen
	r.active = true
	r.stopWait = stopWait
	r.done = done
	r.mu.Unlock()
	go r.run(runCtx, gen, opts, socksPort, emit, done)
	return nil
}

func (r *BoxTunRunner) current(gen uint64) bool {
	r.mu.Lock()
	defer r.mu.Unlock()
	return r.gen == gen
}

func (r *BoxTunRunner) run(ctx context.Context, gen uint64, opts option.Options, socksPort int, emit func(Event), done chan struct{}) {
	defer close(done)
	send := func(ev Event) {
		if r.current(gen) {
			emit(ev)
		}
	}
	// fail ends a start that never produced an instance; a superseded one stays silent.
	fail := func(reason string) {
		r.mu.Lock()
		if r.gen != gen {
			r.mu.Unlock()
			return
		}
		r.active = false
		r.stopWait = nil
		r.hooks.setLogWriter(nil)
		r.mu.Unlock()
		emit(Event{Event: EventTunStopped, Reason: reason})
	}

	if err := r.waitForSocks(ctx, socksPort); err != nil {
		if ctx.Err() == nil {
			fail(err.Error())
		}
		return
	}

	r.mu.Lock()
	if r.gen != gen {
		r.mu.Unlock()
		return
	}
	r.hooks.setLogWriter(&lineWriter{emit: send})
	r.mu.Unlock()

	closer, cancel, err := r.hooks.create(ctx, opts)
	if err != nil {
		if ctx.Err() == nil {
			fail("start tun: " + err.Error())
		}
		return
	}
	r.mu.Lock()
	if r.gen != gen {
		// Stop won the race while the instance was starting: tear it down here.
		r.mu.Unlock()
		_ = closer.Close()
		cancel()
		return
	}
	r.closer, r.cancel = closer, cancel
	r.mu.Unlock()
	send(Event{Event: EventTunReady})
}

func (r *BoxTunRunner) waitForSocks(ctx context.Context, port int) error {
	wait := r.SocksWait
	if wait <= 0 {
		wait = defaultSocksWait
	}
	poll := r.poll
	if poll <= 0 {
		poll = defaultSocksPoll
	}
	addr := net.JoinHostPort("127.0.0.1", strconv.Itoa(port))
	end := time.Now().Add(wait)
	for {
		if err := ctx.Err(); err != nil {
			return err
		}
		if r.hooks.dial(addr, socksDialTimeout) == nil {
			return nil
		}
		remaining := time.Until(end)
		if remaining <= 0 {
			return fmt.Errorf("timed out after %s waiting for the core's SOCKS port %d", wait, port)
		}
		if poll < remaining {
			remaining = poll
		}
		timer := time.NewTimer(remaining)
		select {
		case <-ctx.Done():
			timer.Stop()
			return ctx.Err()
		case <-timer.C:
		}
	}
}

// Stop tears down whatever is running or starting. Closing the instance removes
// the utun and its routes. It is safe to call repeatedly and concurrently.
func (r *BoxTunRunner) Stop() error {
	r.mu.Lock()
	r.gen++
	stopWait, closer, cancel, done := r.stopWait, r.closer, r.cancel, r.done
	r.stopWait, r.closer, r.cancel, r.done = nil, nil, nil, nil
	r.active = false
	r.mu.Unlock()

	var err error
	if closer != nil {
		err = closer.Close()
		if cancel != nil {
			cancel()
		}
	}
	if stopWait != nil {
		stopWait() // aborts the SOCKS wait or an in-flight create
	}
	if done != nil {
		select {
		case <-done:
		case <-time.After(stopWaitBound):
		}
	}
	r.hooks.setLogWriter(nil)
	return err
}

// lineWriter turns the sing-box log stream into EventLog events: one event per
// line, empty lines dropped, each line capped at maxLogLine bytes (the rest of
// an over-long line is discarded).
type lineWriter struct {
	emit func(Event)

	mu        sync.Mutex
	buf       []byte
	truncated bool
}

func (w *lineWriter) Write(p []byte) (int, error) {
	w.mu.Lock()
	defer w.mu.Unlock()
	for _, b := range p {
		if b == '\n' {
			w.flush()
			continue
		}
		if len(w.buf) < maxLogLine {
			w.buf = append(w.buf, b)
		} else {
			w.truncated = true
		}
	}
	return len(p), nil
}

func (w *lineWriter) flush() {
	line := w.buf
	if w.truncated {
		// Do not end a capped line in the middle of a multi-byte character.
		for i := 0; i < utf8.UTFMax-1 && len(line) > 0 && !utf8.Valid(line); i++ {
			line = line[:len(line)-1]
		}
	}
	text := strings.TrimRight(string(line), "\r")
	w.buf = w.buf[:0]
	w.truncated = false
	if text == "" {
		return
	}
	w.emit(Event{Event: EventLog, Line: text})
}
