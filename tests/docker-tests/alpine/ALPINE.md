ALPINE:

# Spuštění balikar na Alpine Linux

balikar binary je zkompilovaný jako glibc-linked ELF. Alpine Linux používá musl libc,
proto je potřeba několik workaroundů.

## 1. Balíčky nutné navíc oproti Debianu

```sh
apk add --no-cache gcompat libc6-compat libcurl jansson pcre2 zlib ca-certificates
```

- `gcompat` — poskytuje `libc.so.6` a `ld-linux-x86-64.so.2` (glibc compat vrstva)
- `libc6-compat` — symlink `/lib64/ld-linux-x86-64.so.2` pro glibc ELF interpreter
- `libcurl`, `jansson` — přímé runtime deps binary
- `pcre2`, `zlib` — **nelze mountovat bundlované glibc verze** z `proj/lib/`; použijte Alpine balíčky

## 2. fcntl64@GLIBC_2.28

`libsolv.so.1` (glibc-compiled) potřebuje symbol `fcntl64` s verzí `GLIBC_2.28`.
Alpine musl ani `gcompat` 1.1.0 ho neposkytují.

Řešení: zkompilujte drobný shim a injektujte ho přes `LD_PRELOAD`:

```sh
apk add --no-cache gcc musl-dev
cat > /tmp/fcntl64compat.c << 'EOF'
#include <stdarg.h>
#include <fcntl.h>
int fcntl64(int fd, int cmd, ...) {
    va_list a; va_start(a, cmd);
    long r = va_arg(a, long); va_end(a);
    return fcntl(fd, cmd, r);
}
EOF
gcc -shared -fPIC -O2 -o /usr/lib/libfcntl64compat.so /tmp/fcntl64compat.c
export LD_PRELOAD=/usr/lib/libfcntl64compat.so
```

V Dockerfilu je vhodné použít multi-stage build, aby gcc nezůstalo v produkčním image.

## 3. libsolv / libsolvext

Tyto knihovny nejsou v Alpine repech. Mountovat z `proj/lib/`:

```sh
docker run --rm \
  -v /proj/balikar:/usr/local/bin/balikar:ro \
  -v /proj/lib/libsolv.so.1:/usr/lib/libsolv.so.1:ro \
  -v /proj/lib/libsolvext.so.1:/usr/lib/libsolvext.so.1:ro \
  ...
```

## 4. busybox tar — chybějící --wildcards

Busybox `tar` na Alpine nepodporuje `--wildcards`. Pokud balikar interně volá:
```
tar ... --wildcards "*/build.sh"
```
selže extrakce build skriptů. Oprava v `src/balikar/downloader.c` — nahradit wildcards
přesnou cestou `pkg-version/build.sh` (viz commit s opravou busybox tar kompatibility).
