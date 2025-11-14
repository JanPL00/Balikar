#!/bin/bash
set -e
echo "Building hello package"
mkdir -p "$DESTDIR/usr/bin"
echo "#!/bin/bash" > "$DESTDIR/usr/bin/hello"
echo "echo Hello from Balikar!" >> "$DESTDIR/usr/bin/hello"
chmod +x "$DESTDIR/usr/bin/hello"
echo "Build complete!"
