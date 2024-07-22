#include <cstdint>
#include <cassert>
#include "gasnet.h"
#include "gasnet_coll.h"


enum class AMType : std::size_t {
  GenericRequest = 0x0,
  Load,
  Store,
  Count,
};

// Convert AMType to underlying integral type
constexpr auto operator+(AMType e) noexcept {
  return static_cast<std::underlying_type_t<AMType>>(e);
}

// GASNet AM arguments are 32bit, so pointers need to be packed and unpacked for 64bit systems

// Number of arguments required for a pointer
constexpr auto ptrNArgs = sizeof(void*) / sizeof(gex_AM_Arg_t);

// Converts a pointer to hi, lo bits
std::tuple<gex_AM_Arg_t, gex_AM_Arg_t> packPtr(void* ptr) noexcept {
  static_assert(sizeof(void*) == ptrNArgs * sizeof(gex_AM_Arg_t));
  return std::make_tuple(static_cast<gex_AM_Arg_t>(GASNETI_HIWORD(ptr)),
                         static_cast<gex_AM_Arg_t>(GASNETI_LOWORD(ptr)));
}

// Converts hi, lo bits to a pointer
void* unpackPtr(gex_AM_Arg_t hi, gex_AM_Arg_t lo) noexcept {
  static_assert(sizeof(void*) == ptrNArgs * sizeof(gex_AM_Arg_t));
  return reinterpret_cast<void*>(GASNETI_MAKEWORD(hi, lo));
}

// Calculates the number of bytes required for t
template <typename... T>
constexpr std::size_t packedSize(const T&... t) noexcept {
  const auto size = (... + sizeof(t));
  return size;
}

// Packs t in buffer and returns a pointer after the packed data
template <typename... T>
void* pack(void* buffer, const T&... t) noexcept {
  auto p = static_cast<std::byte*>(buffer);
  ((std::memcpy(p, &t, sizeof(t)), p += sizeof(t)), ...);
  return p;
}

// Unpacks t from buffer and returns a pointer after the unpacked data
template <typename... T>
void* unpack(void* buffer, T&... t) noexcept {
  auto p = static_cast<std::byte*>(buffer);
  ((std::memcpy(&t, p, sizeof(t)), p += sizeof(t)), ...);
  return p;
}

// Processes a generic request
void handleRequest(gex_Token_t, void*, size_t);
// Processes a load request
void handleLoad(gex_Token_t, void*, size_t, gex_AM_Arg_t handlePtrHi, gex_AM_Arg_t handlePtrLo);
// Processes a store request
void handleStore(gex_Token_t, void*, size_t, gex_AM_Arg_t handlePtrHi, gex_AM_Arg_t handlePtrLo);

struct {
  std::string_view clientName = "pando-rt";
  std::uint64_t rank{GEX_RANK_INVALID};
  std::uint64_t size{GEX_RANK_INVALID};
  gex_Client_t client{GEX_CLIENT_INVALID};
  gex_EP_t endpoint{GEX_EP_INVALID};
  gex_TM_t team{GEX_TM_INVALID};
  std::atomic<bool> pollingThreadActive{true};
  std::thread pollingThread;

  gex_AM_Entry_t htable[+AMType::Count] = {
      // generic request
      {0, reinterpret_cast<gex_AM_Fn_t>(&handleRequest), (GEX_FLAG_AM_REQUEST | GEX_FLAG_AM_MEDIUM),
       0, nullptr, nullptr},

      // load / store
      {0, reinterpret_cast<gex_AM_Fn_t>(&handleLoad), (GEX_FLAG_AM_REQUEST | GEX_FLAG_AM_MEDIUM),
       ptrNArgs, nullptr, nullptr},
      {0, reinterpret_cast<gex_AM_Fn_t>(&handleStore), (GEX_FLAG_AM_REQUEST | GEX_FLAG_AM_MEDIUM),
       ptrNArgs, nullptr, nullptr},

      // acks
      {0, reinterpret_cast<gex_AM_Fn_t>(&handleLoadAck), (GEX_FLAG_AM_REQREP | GEX_FLAG_AM_MEDIUM),
       ptrNArgs, nullptr, nullptr},
      {0, reinterpret_cast<gex_AM_Fn_t>(&handleAck), (GEX_FLAG_AM_REQREP | GEX_FLAG_AM_SHORT),
       ptrNArgs, nullptr, nullptr},
      {0, reinterpret_cast<gex_AM_Fn_t>(&handleValueAck), (GEX_FLAG_AM_REQREP | GEX_FLAG_AM_MEDIUM),
       ptrNArgs, nullptr, nullptr},
  };
} world;

enum Status : size_t {
  OK = 0x0,
  GASNET_INIT_ERROR = 0x1,
  PANDO_OUT_OF_BOUNDS = 0x2;
  PANDO_BAD_ALLOC = 0x3;
};

using GlobalAddress = void*;

Status remoteLoad8(uint64_t nodeIdx, GlobalAddress srcAddr LoadHandle& handle) {
  if(nodeIdx >= world.size) {
    return PANDO_OUT_OF_BOUNDS;
  }
  const auto requestSize = packedSize(srcAddr, n);
  const gex_Flags_t flags = 0;
  const unsigned int numArgs = 2;

  gex_AM_SrcDesc_t sd = gex_AM_PrepareRequestMedium(world.team, nodeIdx, nullptr,
      requestSize, GEX_EVENT_NOW, flags, numArgs);
  auto buffer = gex_AM_SrcDescAddr(sd);
  if(!buffer) {
    return PANDO_BAD_ALLOC;
  }
  pack(buffer, srcAddr, n);
  auto packedHandlePtr = packPtr(&handle);
  gex_AM_CommitRequestMedium2(sd, world.htable[+AMType::Load].gex_index, requestSize,
                              std::get<0>(packedHandlePtr), std::get<1>(packedHandlePtr));
  return OK;
}

void handleLoad(gex_Token_t token, void* buffer, size_t /*byteCount*/, gex_AM_Arg_t handlePtrHi,
                gex_AM_Arg_t handlePtrLo) {
  // unpack
  GlobalAddress srcAddr;
  std::size_t n;
  assert(n == 8);
  unpack(buffer, srcAddr, n);

  // send reply message with data
  void* srcDataPtr = deglobalify(srcAddr);
  const auto flags = 0;
  if (auto status = gex_AM_ReplyMedium(token, world.htable[+AMType::LoadAck].gex_index, srcDataPtr,
                                       n, GEX_EVENT_NOW, flags, handlePtrHi, handlePtrLo);
      status != GASNET_OK) {

    std::abort();
  }
}

Status remoteStore8(NodeIndex nodeIdx, GlobalAddress dstAddr, const void* srcPtr,
                    AckHandle& handle) {
  constexpr std::uint64_t n = 8;
  if (nodeIdx >= world.size) {
    return OutOfBounds;
  }

  // size payload: number of bytes to write is inferred from byteCount
  const auto requestSize = packedSize(dstAddr) + n;

  // get managed buffer to write the request in
  const gex_Flags_t flags = 0;
  const unsigned int numArgs = 2;
  const auto maxMediumRequest =
      gex_AM_MaxRequestMedium(world.team, nodeIdx.id, nullptr, flags, numArgs);
  if (requestSize > maxMediumRequest) {
    return PANDO_BAD_ALLOC;
  }
  gex_AM_SrcDesc_t sd = gex_AM_PrepareRequestMedium(world.team, nodeIdx.id, nullptr, requestSize,
                                                    requestSize, GEX_EVENT_NOW, flags, numArgs);
  auto buffer = gex_AM_SrcDescAddr(sd);
  if (buffer == nullptr) {
    return PANDO_BAD_ALLOC;
  }

  // pack payload: number of bytes to write is inferred from total byte count
  auto packedDataEnd = pack(buffer, dstAddr);
  std::memcpy(packedDataEnd, srcPtr, n);
  // pack pointer for reply
  auto packedHandlePtr = packPtr(&handle);
  // mark buffer ready for send
  gex_AM_CommitRequestMedium2(sd, world.htable[+AMType::Store].gex_index, requestSize,
                              std::get<0>(packedHandlePtr), std::get<1>(packedHandlePtr));

  return OK;
}

void handleStore(gex_Token_t token, void* buffer, size_t byteCount, gex_AM_Arg_t handlePtrHi,
                gex_AM_Arg_t handlePtrLo) {
  // unpack: payload number of bytes inferred from total byte count
  void* dstAddr;
  const uint64_t* srcDataPtr = unpack(buffer, dstAddr);
  const auto n = byteCount - packedSize(dstAddr);
  assert(n == 8);

  // write data payload to global address
  uint64_t* nativeDstPtr = deglobalify(dstAddr);
  *nativeDstPtr = *srcDataPtr;
  std::atomic_thread_fence(std::memory_order_release);

  sendAck(token, handlePtrHi, handlePtrLo);
}

// Processes an ack for a load
void handleLoadAck(gex_Token_t token, void* buffer, size_t byteCount, gex_AM_Arg_t handlePtrHi,
                   gex_AM_Arg_t handlePtrLo) {
  auto handlePtr = reinterpret_cast<Nodes::LoadHandle*>(unpackPtr(handlePtrHi, handlePtrLo));
  handlePtr->setReady(buffer, byteCount);

  static_cast<void>(token);
}

// Processes an ack. This is just a signal with no payload.
void handleAck(gex_Token_t token, gex_AM_Arg_t handlePtrHi, gex_AM_Arg_t handlePtrLo) {
  auto handlePtr = reinterpret_cast<Nodes::AckHandle*>(unpackPtr(handlePtrHi, handlePtrLo));
  handlePtr->setReady();

  static_cast<void>(token);
}

// Processes an ack with a value
void handleValueAck(gex_Token_t token, void* buffer, size_t /*byteCount*/, gex_AM_Arg_t handlePtrHi,
                    gex_AM_Arg_t handlePtrLo) {
  auto handlePtr = reinterpret_cast<Nodes::ValueHandleBase*>(unpackPtr(handlePtrHi, handlePtrLo));
  handlePtr->setReady(buffer);

  static_cast<void>(token);
}

void processMessages(std::atomic<bool>& pollingActive) {
  // block the thread until stopped, while polling GASNet
  while (pollingActive.load(std::memory_order_relaxed) == true) {
    GASNET_BLOCKUNTIL(pollingActive.load(std::memory_order_relaxed) == false);
  }
}

Status initialize(std::uint64_t num_hosts) {
  auto status = gex_Client_Init(&world.client, &world.endpoint, &world.team,
                                      world.clientName.data(), nullptr, nullptr, 0);
  if(status != GASNET_OK) {
    return GASNET_INIT_ERROR;
  }
  world.rank = gex_TM_QueryRank(world.team);
  world.size = gex_TM_QuerySize(world.team);

  status = gex_EP_RegisterHandlers(world.endpoint, world.htable, sizeof(world.htable)/ sizeof(gex_AM_Entry_t));
  if(status != GASNET_OK) { return GASNET_INIT_ERROR; }
  world.pollingThread = std::thread(processMessages, std::ref(world.pollingThreadActive));
  auto barrierEvent = gex_Coll_BarrerNB(world.team, 0);
  gex_Event_Wait(barrierEvent);
  return OK;
}

Status finalize() {
  world.pollingThreadActive.store(false, std::memory_order_relaxed);
  world.pollingThread.join();
  gasnet_barrier_notify(0, GASNET_BARRIERFLAG_ANONYMOUS);
  gasnet_barrier_wait(0, GASNET_BARRIERFLAG_ANONYMOUS);
  return OK;
}

extern "C" {
  int check_if_global(void* ptr) {
    uintptr_t p = (uintptr_t) ptr;
    return (p >> 48) != 0x0;
  }

  void* deglobalify(void* ptr) {
    uintptr_t p = (uintptr_t) ptr;
    uintptr_t mask = ((uintptr_t)0xFFFF - (world.rank & 0xFFFF)) << 48;
    return (void *) (p & ~mask);
  }

  void* globalify(void* ptr) {
    uintptr_t p = (uintptr_t) ptr;
    uintptr_t mask = ((uintptr_t)0xFFFF - (world.rank & 0xFFFF)) << 48;
    return (void *) (p | mask);
  }

  void __pando__replace_store64(uint64_t val, uint64_t* dst) {
    assert(check_if_global(dst));
    *(uint64_t*) deglobalify(dst) = val;
  }

  void __pando__replace_storeptr(void* val, void** dst) {
    assert(check_if_global(dst));
    *(void**) deglobalify(dst) = val;
  }

  uint64_t __pando__replace_load64(uint64_t* src) {
    return *(uint64_t*) deglobalify(src);
  }

  void* __pando__replace_loadptr(void** src) {
    return *(uint64_t**) deglobalify(src);
  }

}
