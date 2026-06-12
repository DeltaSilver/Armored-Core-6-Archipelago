Building from Source

To compile ac6ap.dll yourself you'll need:


Visual Studio 2022 (Desktop development with C++), building Release | x64.
OpenSSL, installed via vcpkg (openssl:x64-windows-static) — used for
wss:// TLS connections to archipelago.gg.


The project targets the static runtime (/MT) with no precompiled headers. If
the build can't find openssl/ssl.h or the OpenSSL libs, point the include and
library directories at your vcpkg install.

To package the Archipelago world, zip the AC6AP folder (folder as the top-level
entry) and rename it to AC6AP.apworld.
