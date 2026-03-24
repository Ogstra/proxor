package boxmain

import (
	"context"
	"io"
	"os"
	"os/signal"
	"os/user"
	"path/filepath"
	"reflect"
	"strconv"
	"syscall"

	box "github.com/sagernet/sing-box"
	"github.com/sagernet/sing-box/include"
	boxlog "github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/json"
	"github.com/sagernet/sing/common/json/badjson"
	"github.com/sagernet/sing/service/filemanager"
)

var disableColor bool
var platformWriter boxlog.PlatformWriter
var logMinLevel = boxlog.LevelInfo

func SetDisableColor(value bool) {
	disableColor = value
}

func SetLogWriter(writer io.Writer) {
	if isNilWriter(writer) {
		platformWriter = nil
		return
	}
	platformWriter = stdPlatformWriter{writer: writer}
}

func Create(configContent []byte) (*box.Box, context.CancelFunc, error) {
	ctx := baseContext()
	options, err := json.UnmarshalExtendedContext[option.Options](ctx, configContent)
	if err != nil {
		return nil, nil, E.Cause(err, "decode config")
	}
	return createWithOptions(ctx, options)
}

func Main() {
	if err := runMain(os.Args[1:]); err != nil {
		boxlog.Fatal(err)
	}
}

type runArguments struct {
	configPaths       []string
	configDirectories []string
	workingDir        string
}

func runMain(args []string) error {
	runArgs, err := parseRunArgs(args)
	if err != nil {
		return err
	}
	ctx := baseContext()
	if runArgs.workingDir != "" {
		if err := os.MkdirAll(runArgs.workingDir, 0o777); err != nil {
			return E.Cause(err, "create working directory")
		}
		if err := os.Chdir(runArgs.workingDir); err != nil {
			return E.Cause(err, "change working directory")
		}
	}
	options, err := readConfigAndMerge(ctx, runArgs)
	if err != nil {
		return err
	}
	instance, cancel, err := createWithOptions(ctx, options)
	if err != nil {
		return err
	}
	defer cancel()
	signals := make(chan os.Signal, 1)
	signal.Notify(signals, os.Interrupt, syscall.SIGTERM, syscall.SIGHUP)
	defer signal.Stop(signals)
	<-signals
	return instance.Close()
}

func parseRunArgs(args []string) (runArguments, error) {
	var parsed runArguments
	commandSeen := false
	for i := 0; i < len(args); i++ {
		arg := args[i]
		switch arg {
		case "--disable-color":
			disableColor = true
		case "run":
			commandSeen = true
		case "-c", "--config":
			if i+1 >= len(args) {
				return parsed, E.New(arg, " requires a value")
			}
			parsed.configPaths = append(parsed.configPaths, args[i+1])
			i++
		case "-C", "--config-directory":
			if i+1 >= len(args) {
				return parsed, E.New(arg, " requires a value")
			}
			parsed.configDirectories = append(parsed.configDirectories, args[i+1])
			i++
		case "-D", "--directory":
			if i+1 >= len(args) {
				return parsed, E.New(arg, " requires a value")
			}
			parsed.workingDir = args[i+1]
			i++
		default:
			return parsed, E.New("unsupported argument: ", arg)
		}
	}
	if len(args) > 0 && !commandSeen {
		return parsed, E.New("unsupported command")
	}
	if len(parsed.configPaths) == 0 && len(parsed.configDirectories) == 0 {
		parsed.configPaths = append(parsed.configPaths, "config.json")
	}
	return parsed, nil
}

func readConfigAndMerge(ctx context.Context, args runArguments) (option.Options, error) {
	optionsList, err := readConfigs(ctx, args)
	if err != nil {
		return option.Options{}, err
	}
	if len(optionsList) == 1 {
		return optionsList[0], nil
	}
	var merged json.RawMessage
	for _, options := range optionsList {
		merged, err = badjson.MergeJSON(ctx, options.RawMessage, merged, false)
		if err != nil {
			return option.Options{}, E.Cause(err, "merge config")
		}
	}
	var mergedOptions option.Options
	if err := mergedOptions.UnmarshalJSONContext(ctx, merged); err != nil {
		return option.Options{}, E.Cause(err, "unmarshal merged config")
	}
	return mergedOptions, nil
}

func readConfigs(ctx context.Context, args runArguments) ([]option.Options, error) {
	var optionsList []option.Options
	for _, path := range args.configPaths {
		options, err := readConfigAt(ctx, path)
		if err != nil {
			return nil, err
		}
		optionsList = append(optionsList, options)
	}
	for _, directory := range args.configDirectories {
		entries, err := os.ReadDir(directory)
		if err != nil {
			return nil, E.Cause(err, "read config directory at ", directory)
		}
		for _, entry := range entries {
			if entry.IsDir() || filepath.Ext(entry.Name()) != ".json" {
				continue
			}
			options, err := readConfigAt(ctx, filepath.Join(directory, entry.Name()))
			if err != nil {
				return nil, err
			}
			optionsList = append(optionsList, options)
		}
	}
	return optionsList, nil
}

func readConfigAt(ctx context.Context, path string) (option.Options, error) {
	content, err := os.ReadFile(path)
	if err != nil {
		return option.Options{}, E.Cause(err, "read config at ", path)
	}
	options, err := json.UnmarshalExtendedContext[option.Options](ctx, content)
	if err != nil {
		return option.Options{}, E.Cause(err, "decode config at ", path)
	}
	return options, nil
}

func createWithOptions(ctx context.Context, options option.Options) (*box.Box, context.CancelFunc, error) {
	if disableColor {
		if options.Log == nil {
			options.Log = &option.LogOptions{}
		}
		options.Log.DisableColor = true
	}
	if options.Log != nil && options.Log.Level != "" {
		if parsed, err := boxlog.ParseLevel(options.Log.Level); err == nil {
			logMinLevel = parsed
		}
	}
	runCtx, cancel := context.WithCancel(ctx)
	instance, err := box.New(box.Options{
		Context:           runCtx,
		Options:           options,
		PlatformLogWriter: platformWriter,
	})
	if err != nil {
		cancel()
		return nil, nil, E.Cause(err, "create service")
	}
	if err := instance.Start(); err != nil {
		cancel()
		instance.Close()
		return nil, nil, E.Cause(err, "start service")
	}
	return instance, cancel, nil
}

func baseContext() context.Context {
	ctx := context.Background()
	sudoUser := os.Getenv("SUDO_USER")
	sudoUID, _ := strconv.Atoi(os.Getenv("SUDO_UID"))
	sudoGID, _ := strconv.Atoi(os.Getenv("SUDO_GID"))
	if sudoUID == 0 && sudoGID == 0 && sudoUser != "" {
		sudoUserObject, _ := user.Lookup(sudoUser)
		if sudoUserObject != nil {
			sudoUID, _ = strconv.Atoi(sudoUserObject.Uid)
			sudoGID, _ = strconv.Atoi(sudoUserObject.Gid)
		}
	}
	if sudoUID > 0 && sudoGID > 0 {
		ctx = filemanager.WithDefault(ctx, "", "", sudoUID, sudoGID)
	}
	return include.Context(ctx)
}

type stdPlatformWriter struct {
	writer io.Writer
}

func (w stdPlatformWriter) DisableColors() bool {
	return true
}

func (w stdPlatformWriter) WriteMessage(level boxlog.Level, message string) {
	if level > logMinLevel {
		return
	}
	if isNilWriter(w.writer) {
		return
	}
	_, _ = io.WriteString(w.writer, message+"\n")
}

func isNilWriter(writer io.Writer) bool {
	if writer == nil {
		return true
	}
	value := reflect.ValueOf(writer)
	switch value.Kind() {
	case reflect.Chan, reflect.Func, reflect.Interface, reflect.Map, reflect.Pointer, reflect.Slice:
		return value.IsNil()
	default:
		return false
	}
}
