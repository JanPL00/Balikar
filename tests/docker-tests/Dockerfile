# Použijeme aktuální a minimální Alpine Linux image
FROM alpine:latest

# Instalace bash (nezbytné pro CMD ["/bin/bash"])
# Volba --no-cache zajistí, že se nebudou ukládat zbytečné indexy balíčků
RUN apk add --no-cache bash

# Minimální runtime závislosti pro spuštění balikar binary (glibc-linked):
#   gcompat       - vrstva glibc kompatibility (libc.so.6, ld-linux-x86-64.so.2)
#   libc6-compat  - symlink /lib64/ld-linux-x86-64.so.2 pro glibc ELF interpreter
#   libcurl       - libcurl.so.4  (runtime dep balikar binary)
#   jansson       - libjansson.so.4 (runtime dep balikar binary)
#   ca-certificates - ověření HTTPS při stahování balíčků
# Ostatní závislosti (gcc, make, python3, …) balikar doinstaluje sám
# z repozitáře https://raw.githubusercontent.com/JanPL00/balikar-repository/main
RUN apk add --no-cache \
    gcompat \
    libc6-compat \
    libcurl \
    jansson \
    pcre2 \
    zlib \
    ca-certificates \
    gcc \
    g++ \
    make \
    musl-dev \
    libtool \
    autoconf \
    automake \
    xz \
    bzip2 \
    file \
    wget

# fcntl64@GLIBC_2.28 – libsolv.so.1 byl zkompilován s glibc a odkazuje na
# symbol fcntl64 s verzí GLIBC_2.28. Alpine musl ani gcompat 1.1.0 ho
# neposkytují. Vytvoříme drobnou compat shim knihovnu a zavedeme ji přes
# LD_PRELOAD, aby byla dostupná pro libsolv.so.1 při startu balikar.
RUN apk add --no-cache --virtual .fcntl-build gcc musl-dev \
    && printf '%s\n' \
        '#include <stdarg.h>' \
        '#include <fcntl.h>' \
        'int fcntl64(int fd,int cmd,...){va_list a;va_start(a,cmd);long r=va_arg(a,long);va_end(a);return fcntl(fd,cmd,r);}' \
        > /tmp/fcntl64compat.c \
    && gcc -shared -fPIC -O2 -o /usr/lib/libfcntl64compat.so /tmp/fcntl64compat.c \
    && rm /tmp/fcntl64compat.c \
    && apk del .fcntl-build

# Nastav LD_PRELOAD tak, aby libsolv.so.1 vždy našel fcntl64
ENV LD_PRELOAD=/usr/lib/libfcntl64compat.so

# Vytvoření adresářové struktury pro balikar
RUN mkdir -p /var/cache/balikar/downloads \
    && mkdir -p /var/lib/balikar \
    && mkdir -p /etc/balikar \
    && mkdir -p /usr/local/bin

# Konfigurační soubor pro balikar (GitHub repository)
RUN echo "[default]" > /etc/balikar/repos.conf && \
    echo "name = Balikar Repository" >> /etc/balikar/repos.conf && \
    echo "url = https://raw.githubusercontent.com/JanPL00/balikar-repository/main" >> /etc/balikar/repos.conf

# Environment pro testování
ENV PATH=/usr/local/bin:$PATH

# Nastavení pracovního adresáře
WORKDIR /test

# Výchozí příkaz
CMD ["/bin/bash"]
