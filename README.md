# NetherNet

NetherNet php implementation using [CNetherNet](https://github.com/vp817/CNetherNet).

## Requirements

**PHP 8.2+ ZTS** with the **nethernet** extension loaded

To build the extension yourself you need the **CNetherNet** repo next to this one, or pass `-DCNETHERNET_DIR=<path>`, plus **CMake 3.22+** and **OpenSSL**

## Build on Windows

Needs the PHP ZTS devpack at C:/php-sdk/php-devpack (or `-DPHP_DEV_DIR=<path>`)

```
cmake -S ext -B build -G "Visual Studio 18 2026" -A x64 -T v143,version=14.44 -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"
cmake --build build --config Release
```

The dll ends up in build/Release, add `-DPHP_BIN_DIR=<php dir>` to copy it into that php's ext/ after every build

**If php refuses to load it the build id does not match**, check `php -i | findstr "PHP Extension Build"` and pass it with `-DPHP_BUILD_ID`

## Build on Linux

Needs the headers of the same PHP version (`make install-headers` in its source) and build-essential cmake libssl-dev

```
cmake -S ext -B build -DCMAKE_BUILD_TYPE=Release -DPHP_INCLUDE_DIR=<headers>/include/php -DOPENSSL_SSL_LIBRARY=<php7>/lib/libssl.so -DOPENSSL_CRYPTO_LIBRARY=<php7>/lib/libcrypto.so
cmake --build build --target php_nethernet
```

The .so ends up in build/. **Build it on the server itself** (or a distro no newer than it), the .so needs the same glibc. .github/workflows/build.yml does all of it step by step.

## Usage

composer require laith98dev/nethernet

```php
$server = new NetherNetServer("0.0.0.0:19132", new ServerInformation("My Server", 2177, "1.26.50"), $listener);
$server->start();

// every tick
$server->tick();
```

$listener is a NetherNetListener, it gets onConnect, onPacket and onDisconnect.

NetherNetClient connects to other NetherNet servers like the minecraft client does (for proxies).

## Debugging

Envorinment **NN_DEBUG** **=1** or **=2** or **=3** prints what the C library is doing.

## License

MIT, check LICENSE
