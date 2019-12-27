# RawQuic
This project transmits raw quic data on client side without http2/spdy, but in QuicTransport protocol.
[[Draft]](https://tools.ietf.org/html/draft-vvv-webtransport-quic)
[[Design]](https://docs.google.com/document/d/1UgviRBnZkMUq4OKcsAJvIQFX6UCXeCbOtX_wMgwD_es/edit#).

This function provides ability to replace TCP with QUIC for some app, such as "RTMP OVER QUIC" which already implemented by Tencent not long before.

There's a simple QuicTransport echo server "quic_transport_simple_server" in chromium, and our formal support will be added on [nginx-quic](https://github.com/evansun922/nginx-quic) RTMP module with "RTMP OVER QUIC" function soon.

## Chromium commit
4f9161f264b29ca04c466e37ae91497d3420e9cc, after tag 81.0.4008.1.

You should build chromium and be able to continue to follow steps.

## Build librawquic

### Patch RawQuic to chromium
```
mv RAW_QUIC_REPOSITORY_DIR/src/raw_quic CHROMIUM_ROOT_DIR/src/net/quic/
```

### Patch net/BUILD.gn
Add codes below to net/BUILD.gn.
```
shared_library("librawquic") {
  sources = [
    "quic/raw_quic/streambuf/basic_streambuf.hpp",
    "quic/raw_quic/streambuf/basic_streambuf_fwd.hpp",
    "quic/raw_quic/streambuf/buffer.hpp",
    "quic/raw_quic/streambuf/streambuf.hpp",
    "quic/raw_quic/raw_quic.cc",
    "quic/raw_quic/raw_quic.h",
    "quic/raw_quic/raw_quic_api.cc",
    "quic/raw_quic/raw_quic_api.h",
    "quic/raw_quic/raw_quic_context.cc",
    "quic/raw_quic/raw_quic_context.h",
    "quic/raw_quic/raw_quic_session.cc",
    "quic/raw_quic/raw_quic_session.h",
  ]
  deps = [
    ":net",
    ":simple_quic_tools",
    "//base",
    "//third_party/boringssl",
    "//third_party/protobuf:protobuf_lite",
  ]
  defines = [ "RAW_QUIC_EXPORTS", "RAW_QUIC_SHARED_LIBRARY" ]
}
```

### Generate project on specified platform
```
gn gen out/Debug --args="is_debug=true target_cpu=\"x86\""
```

### Build
ninja -C out\Debug librawquic

## Test

### Build and run server demo
Build test echo server quic_transport_simple_server and run.
```
ninja -C out\Debug quic_transport_simple_server
```

### Build and run client demo
See sample code test/raw_quic_test.cpp

Enjoy it.
