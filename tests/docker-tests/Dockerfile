FROM debian:bookworm-slim

# gcc + make   - kompilátor a build systém (bez --no-install-recommends → apt stáhne i libc6-dev)
# bash         - build.sh skripty používají #!/bin/bash
# bzip2        - extrakce .tar.bz2 (nmap, libssh2, ...)
# ca-certificates - ověření HTTPS certifikátů
# Ostatní nástroje (flex, perl, xz-utils, ...) nainstaluje balikar sám z repozitáře via mkdeps.
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    bash \
    bzip2 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Adresářová struktura balikar
RUN mkdir -p /var/cache/balikar/downloads \
             /var/lib/balikar \
             /etc/balikar \
             /usr/local/bin


# Konfigurační soubor (GitHub repository)
RUN printf '[default]\nname = Balikar Repository\nurl = https://raw.githubusercontent.com/JanPL00/balikar-repository/main\n' \
    > /etc/balikar/repos.conf

# Statická binárka — build kontextem je kořen projektu (viz sestavení níže)
COPY balikar-static /usr/local/bin/balikar
RUN chmod +x /usr/local/bin/balikar

ENV PATH=/usr/local/bin:$PATH
WORKDIR /test
CMD ["/bin/bash"]
