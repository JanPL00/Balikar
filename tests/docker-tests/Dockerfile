FROM debian:bookworm-slim

# Minimální runtime závislosti pro balikar binary (glibc-linked ELF):
#   libcurl4        - libcurl.so.4  (přímá runtime dep balikar binary)
#   libjansson4     - libjansson.so.4 (přímá runtime dep balikar binary)
#   libpcre2-posix3 - libpcre2-posix.so.3 (dep libsolv.so.1 - není v Debian repech)
#   ca-certificates - ověření HTTPS certifikátů při stahování balíčků
# libsolv/libsolvext nejsou v Debian repech - mountovat z proj/lib/ při spuštění.
# Ostatní závislosti (gcc, make, …) doinstaluje balikar sám z repozitáře.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libcurl4 \
    libjansson4 \
    libpcre2-posix3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

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
