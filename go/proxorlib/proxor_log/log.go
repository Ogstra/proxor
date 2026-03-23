package proxor_log

import (
	"fmt"
	"io"
	"log"
	"os"
	"runtime"

	"github.com/Ogstra/proxorlib/proxor_common"
	"github.com/Ogstra/proxorlib/syscallw"
)

var LogWriter *logWriter
var LogWriterDisable = false
var TruncateOnStart = true
var NB4AGuiLogWriter io.Writer

func SetupLog(maxSize int, path string) (err error) {
	if LogWriter != nil {
		return
	}

	var f *os.File
	f, err = os.OpenFile(path, os.O_RDWR|os.O_APPEND|os.O_CREATE, 0644)
	if err == nil {
		fd := int(f.Fd())
		if TruncateOnStart {
			syscallw.Flock(fd, syscallw.LOCK_EX)
			// Check if need truncate
			if size, _ := f.Seek(0, io.SeekEnd); size > int64(maxSize) {
				// read oldBytes for maxSize
				f.Seek(-int64(maxSize), io.SeekCurrent)
				oldBytes, err := io.ReadAll(f)
				if err == nil {
					// truncate file
					if runtime.GOOS == "windows" {
						f.Close()
						os.Remove(path)
						// reopen file
						f, err = os.OpenFile(path, os.O_RDWR|os.O_APPEND|os.O_CREATE, 0644)
					} else {
						err = f.Truncate(0)
					}
					// write oldBytes
					if err == nil {
						f.Write(oldBytes)
					}
				}
			}
			syscallw.Flock(fd, syscallw.LOCK_UN)
		}
		if proxor_common.RunMode == proxor_common.RunMode_ProxorAndroid {
			// redirect stderr
			syscallw.Dup3(fd, int(os.Stderr.Fd()), 0)
		}
	}

	if err != nil {
		err = fmt.Errorf("error open log: %v", err)
		log.Println(err)
	}

	//
	LogWriter = &logWriter{}
	if proxor_common.RunMode == proxor_common.RunMode_ProxorAndroid {
		LogWriter.writers = []io.Writer{NB4AGuiLogWriter, f}
	} else {
		LogWriter.writers = []io.Writer{os.Stdout, f}
	}
	// setup std log
	log.SetFlags(log.LstdFlags | log.LUTC)
	log.SetOutput(LogWriter)

	return
}

type logWriter struct {
	writers []io.Writer
}

func (w *logWriter) Write(p []byte) (int, error) {
	if LogWriterDisable {
		return len(p), nil
	}

	for _, w := range w.writers {
		if w == nil {
			continue
		}
		if f, ok := w.(*os.File); ok {
			fd := int(f.Fd())
			syscallw.Flock(fd, syscallw.LOCK_EX)
			f.Write(p)
			syscallw.Flock(fd, syscallw.LOCK_UN)
		} else {
			w.Write(p)
		}
	}

	return len(p), nil
}

func (w *logWriter) Truncate() {
	for _, w := range w.writers {
		if w == nil {
			continue
		}
		if f, ok := w.(*os.File); ok {
			_ = f.Truncate(0)
		}
	}
}

func (w *logWriter) Close() error {
	for _, w := range w.writers {
		if w == nil {
			continue
		}
		if f, ok := w.(*os.File); ok {
			_ = f.Close()
		}
	}
	return nil
}
