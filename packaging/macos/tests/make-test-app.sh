#!/bin/bash
# Builds an ad-hoc signed THROWAWAY macOS app bundle (and optionally its zip) for the relauncher tests.
# It is never Proxor: own bundle id (io.github.Ogstra.Proxor.updtest by default), LSUIElement, tiny C program.
#
# usage: make-test-app.sh <dir> <Name.app> <version> [options]
#   --id <bundle id>       CFBundleIdentifier (default io.github.Ogstra.Proxor.updtest)
#   --exit <code>          the program exits with <code> right after logging (default: sleeps 20 s)
#   --arch <arch>          compile for arm64 or x86_64 (default: this Mac's architecture)
#   --min <macOS>          LSMinimumSystemVersion (default 12.0)
#   --result-file <file>   at start the program appends "result=<first line of that file>" to its log
#   --helper-exec          also builds Contents/MacOS/helper (logs "<ver> <pid> helper", sleeps 30 s);
#                          the main program starts it before doing anything else
#   --zip <out.zip>        also writes the zip with the ditto flags of libs/package_macos.sh
#
# The program appends "<version> <pid> start <argv...>" to "<dir of the bundle>/<Name without .app>-run.log"
# (derived from its own path at run time, so a bundle moved elsewhere logs next to its new place).
# bash 3.2 compatible. Needs a C compiler (xcrun clang, else /usr/bin/cc).
set -eu

usage() { echo "usage: $0 <dir> <Name.app> <version> [--id ID] [--exit N] [--arch A] [--min V] [--result-file F] [--helper-exec] [--zip Z]" >&2; exit 2; }
[ $# -ge 3 ] || usage
DIR=$1
NAME=$2
VERSION=$3
shift 3
BID=io.github.Ogstra.Proxor.updtest
EXITCODE=""
ARCH=""
MINOS=12.0
RESULT_FILE=""
HELPER=0
ZIP=""
while [ $# -gt 0 ]; do
  case $1 in
    --id) [ $# -ge 2 ] || usage; BID=$2; shift 2 ;;
    --exit) [ $# -ge 2 ] || usage; EXITCODE=$2; shift 2 ;;
    --arch) [ $# -ge 2 ] || usage; ARCH=$2; shift 2 ;;
    --min) [ $# -ge 2 ] || usage; MINOS=$2; shift 2 ;;
    --result-file) [ $# -ge 2 ] || usage; RESULT_FILE=$2; shift 2 ;;
    --helper-exec) HELPER=1; shift ;;
    --zip) [ $# -ge 2 ] || usage; ZIP=$2; shift 2 ;;
    *) usage ;;
  esac
done
case $NAME in
  *.app) ;;
  *) echo "make-test-app: name must end in .app" >&2; exit 2 ;;
esac
[ -d "$DIR" ] || { echo "make-test-app: $DIR is not a directory" >&2; exit 2; }

APP="$DIR/$NAME"
[ ! -e "$APP" ] || { echo "make-test-app: $APP exists" >&2; exit 2; }
SRC=$(mktemp -d "${TMPDIR:-/tmp}/updtest-src.XXXXXX")
trap 'rm -rf "$SRC"' EXIT

cat > "$SRC/app.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#ifndef APP_VERSION
#define APP_VERSION "0"
#endif

/* "<parent>/<Name>-run.log" for the bundle this executable runs from (argv[0] = <bundle>/Contents/MacOS/<exe>). */
static int bundle_paths(const char *argv0, char *logpath, size_t n, char *macos, size_t m) {
  char buf[4096];
  char *p, *slash, *name;
  strncpy(buf, argv0, sizeof buf - 1);
  buf[sizeof buf - 1] = 0;
  p = strstr(buf, ".app/Contents/MacOS/");
  if (!p) return -1;
  p[4] = 0; /* buf = <parent>/<Name>.app */
  slash = strrchr(buf, '/');
  if (!slash) return -1;
  *slash = 0;
  name = slash + 1;
  name[strlen(name) - 4] = 0; /* drop .app */
  snprintf(logpath, n, "%s/%s-run.log", buf, name);
  snprintf(macos, m, "%s.app/Contents/MacOS", buf[0] ? buf : "");
  return 0;
}

int main(int argc, char **argv) {
  char logpath[4096], macos[4096];
  FILE *f;
  int i;
  if (bundle_paths(argv[0], logpath, sizeof logpath, macos, sizeof macos) != 0) return 3;
#ifdef HELPER_PROGRAM
  f = fopen(logpath, "a");
  if (f) { fprintf(f, "%s %d helper\n", APP_VERSION, (int)getpid()); fclose(f); }
  sleep(30);
  return 0;
#else
#ifdef SPAWN_HELPER
  {
    char helper[4200];
    char *p, *q;
    char bundle[4096];
    strncpy(bundle, argv[0], sizeof bundle - 1);
    bundle[sizeof bundle - 1] = 0;
    q = strstr(bundle, "/MacOS/");
    if (q) {
      q[7] = 0;
      snprintf(helper, sizeof helper, "%shelper", bundle);
      (void)p;
      if (fork() == 0) {
        setsid();
        execl(helper, helper, (char *)NULL);
        _exit(1);
      }
    }
  }
#endif
  f = fopen(logpath, "a");
  if (f) {
    fprintf(f, "%s %d start", APP_VERSION, (int)getpid());
    for (i = 1; i < argc; i++) fprintf(f, " %s", argv[i]);
    fprintf(f, "\n");
#ifdef RESULT_FILE_PATH
    {
      char line[1024];
      FILE *r = fopen(RESULT_FILE_PATH, "r");
      line[0] = 0;
      if (r) {
        if (fgets(line, sizeof line, r)) line[strcspn(line, "\n")] = 0;
        fclose(r);
      }
      fprintf(f, "result=%s\n", line);
    }
#endif
    fclose(f);
  }
#ifdef EXIT_CODE
  return EXIT_CODE;
#else
  sleep(20);
  return 0;
#endif
#endif
}
EOF

CC="xcrun clang"
if ! xcrun --find clang >/dev/null 2>&1; then CC=/usr/bin/cc; fi
ARCHFLAG=""
[ -z "$ARCH" ] || ARCHFLAG="-arch $ARCH"

mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
# shellcheck disable=SC2086
$CC $ARCHFLAG -O1 -DAPP_VERSION="\"$VERSION\"" ${EXITCODE:+-DEXIT_CODE=$EXITCODE} \
  ${RESULT_FILE:+-DRESULT_FILE_PATH="\"$RESULT_FILE\""} $( [ "$HELPER" = 1 ] && echo -DSPAWN_HELPER ) \
  -o "$APP/Contents/MacOS/app" "$SRC/app.c"
if [ "$HELPER" = 1 ]; then
  # shellcheck disable=SC2086
  $CC $ARCHFLAG -O1 -DAPP_VERSION="\"$VERSION\"" -DHELPER_PROGRAM -o "$APP/Contents/MacOS/helper" "$SRC/app.c"
fi
echo "throwaway data $VERSION" > "$APP/Contents/Resources/data.txt"

cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleIdentifier</key><string>$BID</string>
  <key>CFBundleName</key><string>UpdTest</string>
  <key>CFBundleExecutable</key><string>app</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>LSMinimumSystemVersion</key><string>$MINOS</string>
  <key>LSUIElement</key><true/>
</dict>
</plist>
EOF

codesign --force --deep --sign - "$APP" >/dev/null 2>&1
codesign --verify --deep --strict "$APP"

if [ -n "$ZIP" ]; then
  rm -f "$ZIP"
  # the exact flags of libs/package_macos.sh
  ditto -c -k --keepParent --norsrc --noextattr --noqtn --noacl "$APP" "$ZIP"
fi
