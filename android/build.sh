#!/bin/bash
# Offline Android build (no Gradle): NDK clang -> libpinball.so, aapt2 -> APK, zipalign + apksigner.
# Usage: android/build.sh [--install]
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SDK="${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}"
NDK="${ANDROID_NDK_ROOT:-$SDK/ndk/29.0.14206865}"
BT="$SDK/build-tools/36.0.0"
PLATFORM="$SDK/platforms/android-35/android.jar"
export JAVA_HOME="${JAVA_HOME:-/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home}"
export PATH="$JAVA_HOME/bin:$PATH"
API=26
ABI=arm64-v8a
CXX="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android${API}-clang++"
CC="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android${API}-clang"
GLUE="$NDK/sources/android/native_app_glue"

OUT="$HERE/build"
OBJ="$OUT/obj/$ABI"
mkdir -p "$OBJ" "$OUT/apk/lib/$ABI" "$OUT/assets"

SRCS=(
  "$ROOT/src/dat_file.cpp"
  "$ROOT/src/exe_resources.cpp"
  "$ROOT"/src/engine/*.cpp
  "$ROOT"/src/engine/control/*.cpp
  "$HERE/jni/android_main.cpp"
)
CXXFLAGS=(-std=c++17 -O2 -fPIC -DANDROID -I"$ROOT/src" -I"$GLUE" -Wall -Wno-unused-parameter)

echo "== compile"
OBJS=()
pids=()
for s in "${SRCS[@]}"; do
  o="$OBJ/$(echo "${s#$ROOT/}" | tr '/' '_').o"
  OBJS+=("$o")
  if [[ ! -f "$o" || "$s" -nt "$o" || -n "$(find "$ROOT/src" "$HERE/jni" -name '*.h' -newer "$o" 2>/dev/null | head -1)" ]]; then
    "$CXX" "${CXXFLAGS[@]}" -c "$s" -o "$o" &
    pids+=($!)
  fi
done
"$CC" -O2 -fPIC -c "$GLUE/android_native_app_glue.c" -o "$OBJ/glue.o" &
pids+=($!)
for p in "${pids[@]}"; do wait "$p"; done

echo "== link"
"$CXX" -shared -o "$OUT/apk/lib/$ABI/libpinball.so" "${OBJS[@]}" "$OBJ/glue.o" \
  -static-libstdc++ -u ANativeActivity_onCreate -Wl,--no-undefined -Wl,-z,max-page-size=16384 \
  -landroid -llog -laaudio -lm
"$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip" --strip-unneeded "$OUT/apk/lib/$ABI/libpinball.so"

if [[ ! -f "$ROOT/3dPinballWindows/PINBALL.DAT" || ! -f "$ROOT/3dPinballWindows/Pinball.exe" ]]; then
  echo "Data game belum ada. Lihat 3dPinballWindows/README.md" >&2; exit 1
fi
echo "== assets"
rm -rf "$OUT/assets"; mkdir -p "$OUT/assets"
for f in "$ROOT"/3dPinballWindows/*; do
  case "$(basename "$f")" in
    *.bmp) ;;  # not needed at runtime (Pinball.exe is: strings + message font)
    *) cp "$f" "$OUT/assets/" ;;
  esac
done

echo "== package"
rm -f "$OUT/unsigned.apk" "$OUT/aligned.apk" "$OUT/pinball.apk"
# -0 "" : store everything uncompressed (WAV/DAT/MID read as plain files; .so must be uncompressed+aligned)
"$BT/aapt2" link -I "$PLATFORM" --manifest "$HERE/AndroidManifest.xml" -A "$OUT/assets" \
  --min-sdk-version 26 --target-sdk-version 35 -0 "" -o "$OUT/unsigned.apk"
(cd "$OUT/apk" && zip -q -0 -r "$OUT/unsigned.apk" lib)
"$BT/zipalign" -f -P 16 4 "$OUT/unsigned.apk" "$OUT/aligned.apk"

KS="$HERE/debug.keystore"
if [[ ! -f "$KS" ]]; then
  keytool -genkeypair -keystore "$KS" -storepass android -keypass android -alias androiddebugkey \
    -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Android Debug,O=Android,C=US" >/dev/null
fi
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --key-pass pass:android --ks-key-alias androiddebugkey \
  --out "$OUT/pinball.apk" "$OUT/aligned.apk"
"$BT/apksigner" verify "$OUT/pinball.apk"
rm -f "$OUT/unsigned.apk" "$OUT/aligned.apk" "$OUT/pinball.apk.idsig"
echo "APK: $OUT/pinball.apk"

if [[ "${1:-}" == "--install" ]]; then
  ADB="$SDK/platform-tools/adb"
  "$ADB" install -r "$OUT/pinball.apk"
  "$ADB" shell am start -n com.spacecadet.pinball/android.app.NativeActivity
fi
