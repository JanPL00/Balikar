# Dockerfile pro testování Balikar Package Manageru
# Minimální prostředí s pouze nezbytnými závislostmi

FROM alpine:latest

# Metadata
LABEL description="Balikar Package Manager - Testing Environment"
LABEL version="1.0"

# Instalace pouze nezbytných build závislostí pro balikar
# Alpine používá apk, ale neinstalujeme žádné balíčky přes něj
RUN apk add --no-cache \
    gcc \
    g++ \
    cmake \
    make \
    musl-dev \
    linux-headers \
    jansson-dev \
    curl-dev \
    libarchive-dev \
    bash \
    git

# Vytvoření adresářové struktury pro balikar
RUN mkdir -p /var/cache/balikar/downloads \
    && mkdir -p /var/lib/balikar \
    && mkdir -p /etc/balikar \
    && mkdir -p /usr/local/bin

# Nastavení pracovního adresáře
WORKDIR /balikar

# Kopírování zdrojových souborů (když budeme buildovat v containeru)
# COPY . /balikar/

# Výchozí příkaz
CMD ["/bin/bash"]

# Poznámky:
# - Alpine je minimální distribuce (~5MB)
# - Obsahuje pouze základní GNU coreutils
# - Ideální pro testování instalace balíčků od nuly
# - apk package manager je v systému, ale nekonfliktuje s balikarem
