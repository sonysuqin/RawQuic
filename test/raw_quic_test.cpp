#include <stdio.h>
#include <string.h>
#include <thread>

#include "raw_quic_api.h"

#define TEST_CLOSE_ON_ANOTHER_THREAD
#define MEM_LEAK_CHECK

#ifdef MEM_LEAK_CHECK
#ifdef WIN32
#include <vld.h>
#endif
#endif

void TestConnectCallback(RawQuicHandle handle,
                         RawQuicError* error,
                         void* opaque) {
  printf("ConnectCallback %d %d %d\n", error->error, error->net_error,
         error->quic_error);
}

void TestErrorCallback(RawQuicHandle handle,
                       RawQuicError* error,
                       void* opaque) {
  printf("ErrorCallback %d %d %d\n", error->error, error->net_error,
         error->quic_error);
}

void TestCanReadCallback(RawQuicHandle handle, uint32_t size, void* opaque) {
  printf("TestCanReadCallback %u\n", size);
}

int main(int argc, char** argv) {
  RawQuicCallbacks callbacks;
  callbacks.connect_callback = TestConnectCallback;
  callbacks.error_callback = TestErrorCallback;
  callbacks.can_read_callback = TestCanReadCallback;

  RawQuicHandle handle = nullptr;

  do {
    handle = RawQuicOpen(callbacks, nullptr, true);
    if (handle == 0) {
      printf("RawQuicOpen failed.\n");
      break;
    }

    int32_t ret = RawQuicConnect(handle, "testlive.hd.sohu.com", 4444, 0);
    printf("RawQuicConnect return %d.\n", ret);
    if (ret != RAW_QUIC_ERROR_CODE_SUCCESS) {
      break;
    }

    char input[1024] = {0};
    while (1) {
      gets_s(input, 1024);
      int32_t len = strlen(input);
      if (strncmp(input, "exit", 4) == 0 || strncmp(input, "quit", 4) == 0) {
        break;
      }

      if (len == 0) {
        printf("Invalid input.\n");
        continue;
      }

      ret = RawQuicSend(handle, (uint8_t*)input, (uint32_t)len);
      if (ret < 0) {
        printf("RawQuicSend failed %d.\n", ret);
        break;
      }

      printf("Send : %s\n", input);

      char buffer[1024] = {0};
      ret = RawQuicRecv(handle, (uint8_t*)buffer, sizeof(buffer), 5000);
      if (ret < 0) {
        printf("RawQuicRecv failed %d.\n", ret);
        break;
      }

      printf("Recv : %s\n", buffer);
    }
  } while (0);

#ifdef TEST_CLOSE_ON_ANOTHER_THREAD
  std::thread close_thread([handle]() {
    if (handle > 0) {
      RawQuicClose(handle);
    }
    printf("Handle closed on another thread.\n");
  });
  close_thread.join();
#else
  if (handle > 0) {
    RawQuicClose(handle);
  }
  printf("Handle closed on original thread.\n");
#endif

  getchar();
  return 0;
}
