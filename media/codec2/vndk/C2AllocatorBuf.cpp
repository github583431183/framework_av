/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "C2AllocatorBuf"
#include <list>
#include <utils/Log.h>

#include <BufferAllocator/BufferAllocator.h>
#include <linux/ion.h>
#include <sys/mman.h>
#include <unistd.h> // getpagesize, size_t, close, dup

#include <C2AllocatorBuf.h>
#include <C2Buffer.h>
#include <C2Debug.h>
#include <C2ErrnoUtils.h>

#ifdef __ANDROID_APEX__
#include <android-base/properties.h>
#endif

namespace android {

/* =========================== BUFFER HANDLE =========================== */
/**
 * Buffer handle
 *
 * Stores dmabuf fd & metadata
 *
 * This handle will not capture mapped fd-s as updating that would require a
 * global mutex.
 */

struct C2HandleBuf : public C2Handle {
  C2HandleBuf(int bufferFd, size_t size)
      : C2Handle(cHeader), mFds{bufferFd}, mInts{int(size & 0xFFFFFFFF),
                                                 int((uint64_t(size) >> 32) &
                                                     0xFFFFFFFF),
                                                 kMagic} {}

  static bool isValid(const C2Handle *const o);

  int bufferFd() const { return mFds.mBuffer; }
  size_t size() const {
    return size_t(unsigned(mInts.mSizeLo)) |
           size_t(uint64_t(unsigned(mInts.mSizeHi)) << 32);
  }

protected:
  struct {
    int mBuffer; // dmabuf fd
  } mFds;
  struct {
    int mSizeLo; // low 32-bits of size
    int mSizeHi; // high 32-bits of size
    int mMagic;
  } mInts;

private:
  typedef C2HandleBuf _type;
  enum {
    kMagic = '\xc2io\x00',
    numFds = sizeof(mFds) / sizeof(int),
    numInts = sizeof(mInts) / sizeof(int),
    version = sizeof(C2Handle)
  };
  // constexpr static C2Handle cHeader = { version, numFds, numInts, {} };
  const static C2Handle cHeader;
};

const C2Handle C2HandleBuf::cHeader = {
    C2HandleBuf::version, C2HandleBuf::numFds, C2HandleBuf::numInts, {}};

// static
bool C2HandleBuf::isValid(const C2Handle *const o) {
  if (!o || memcmp(o, &cHeader, sizeof(cHeader))) {
    return false;
  }
  const C2HandleBuf *other = static_cast<const C2HandleBuf *>(o);
  return other->mInts.mMagic == kMagic;
}

/* =========================== DMABUF ALLOCATION =========================== */
class C2AllocationBuf : public C2LinearAllocation {
public:
  /* Interface methods */
  virtual c2_status_t map(size_t offset, size_t size, C2MemoryUsage usage,
                          C2Fence *fence, void **addr /* nonnull */) override;
  virtual c2_status_t unmap(void *addr, size_t size, C2Fence *fenceFd) override;
  virtual ~C2AllocationBuf() override;
  virtual const C2Handle *handle() const override;
  virtual id_t getAllocatorId() const override;
  virtual bool
  equals(const std::shared_ptr<C2LinearAllocation> &other) const override;

  // internal methods
  C2AllocationBuf(BufferAllocator &alloc, size_t size, size_t align,
                  C2MemoryUsage usage, C2Allocator::id_t id);
  C2AllocationBuf(size_t size, int shareFd, C2Allocator::id_t id);

  c2_status_t status() const;

protected:
  virtual c2_status_t mapInternal(size_t mapSize, size_t mapOffset,
                                  size_t alignmentBytes, int prot, int flags,
                                  void **base, void **addr) {
    c2_status_t err = C2_OK;
    *base = mmap(nullptr, mapSize, prot, flags, mHandle.bufferFd(), mapOffset);
    ALOGV("mmap(size = %zu, prot = %d, flags = %d, mapFd = %d, offset = %zu) "
          "returned (%d)",
          mapSize, prot, flags, mHandle.bufferFd(), mapOffset, errno);
    if (*base == MAP_FAILED) {
      *base = *addr = nullptr;
      err = c2_map_errno<EINVAL>(errno);
    } else {
      *addr = (uint8_t *)*base + alignmentBytes;
    }
    return err;
  }

  C2Allocator::id_t mId;
  C2HandleBuf mHandle;
  c2_status_t mInit;
  struct Mapping {
    void *addr;
    size_t alignmentBytes;
    size_t size;
  };
  std::list<Mapping> mMappings;

  // TODO: we could make this encapsulate shared_ptr and copiable
  C2_DO_NOT_COPY(C2AllocationBuf);
};

c2_status_t C2AllocationBuf::map(size_t offset, size_t size,
                                 C2MemoryUsage usage, C2Fence *fence,
                                 void **addr) {
  (void)fence; // TODO: wait for fence
  *addr = nullptr;
  if (!mMappings.empty()) {
    ALOGV("multiple map");
    // TODO: technically we should return DUPLICATE here, but our block views
    // don't actually unmap, so we end up remapping an ion buffer multiple
    // times.
    //
    // return C2_DUPLICATE;
  }
  if (size == 0) {
    return C2_BAD_VALUE;
  }

  int prot = PROT_NONE;
  int flags = MAP_SHARED;
  if (usage.expected & C2MemoryUsage::CPU_READ) {
    prot |= PROT_READ;
  }
  if (usage.expected & C2MemoryUsage::CPU_WRITE) {
    prot |= PROT_WRITE;
  }

  size_t alignmentBytes = offset % PAGE_SIZE;
  size_t mapOffset = offset - alignmentBytes;
  size_t mapSize = size + alignmentBytes;
  Mapping map = {nullptr, alignmentBytes, mapSize};

  c2_status_t err = mapInternal(mapSize, mapOffset, alignmentBytes, prot, flags,
                                &(map.addr), addr);
  if (map.addr) {
    mMappings.push_back(map);
  }
  return err;
}

c2_status_t C2AllocationBuf::unmap(void *addr, size_t size, C2Fence *fence) {
  if (mMappings.empty()) {
    ALOGD("tried to unmap unmapped buffer");
    return C2_NOT_FOUND;
  }
  for (auto it = mMappings.begin(); it != mMappings.end(); ++it) {
    if (addr != (uint8_t *)it->addr + it->alignmentBytes ||
        size + it->alignmentBytes != it->size) {
      continue;
    }
    int err = munmap(it->addr, it->size);
    if (err != 0) {
      ALOGD("munmap failed");
      return c2_map_errno<EINVAL>(errno);
    }
    if (fence) {
      *fence = C2Fence(); // not using fences
    }
    (void)mMappings.erase(it);
    ALOGV("successfully unmapped: %d", mHandle.bufferFd());
    return C2_OK;
  }
  ALOGD("unmap failed to find specified map");
  return C2_BAD_VALUE;
}

c2_status_t C2AllocationBuf::status() const { return mInit; }

C2Allocator::id_t C2AllocationBuf::getAllocatorId() const { return mId; }

bool C2AllocationBuf::equals(
    const std::shared_ptr<C2LinearAllocation> &other) const {
  if (!other || other->getAllocatorId() != getAllocatorId()) {
    return false;
  }
  // get user handle to compare objects
  std::shared_ptr<C2AllocationBuf> otherAsBuf =
      std::static_pointer_cast<C2AllocationBuf>(other);
  return mHandle.bufferFd() == otherAsBuf->mHandle.bufferFd();
}

const C2Handle *C2AllocationBuf::handle() const { return &mHandle; }

C2AllocationBuf::~C2AllocationBuf() {
  if (!mMappings.empty()) {
    ALOGD("Dangling mappings!");
    for (const Mapping &map : mMappings) {
      (void)munmap(map.addr, map.size);
    }
  }
  if (mInit == C2_OK) {
    native_handle_close(&mHandle);
  }
}

C2AllocationBuf::C2AllocationBuf(BufferAllocator &alloc, size_t size,
                                 size_t align, C2MemoryUsage usage,
                                 C2Allocator::id_t id)
    : C2LinearAllocation(size), mHandle(-1, 0) {

  int bufferFd = -1;
  size_t alignedSize = align == 0 ? size : (size + align - 1) & ~(align - 1);
  int ret = 0;

  if (!(usage.expected & (C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE)))
    bufferFd = alloc.Alloc("system-uncached", alignedSize, 0);

  if (bufferFd !=
      -EEXIST) /* If system-uncached doesn't exist, fall back to system */
    bufferFd = alloc.Alloc("system", alignedSize, 0);

  if (bufferFd < 0)
    ret = bufferFd;

  mHandle = C2HandleBuf(bufferFd, alignedSize);
  mId = id;
  mInit = c2_status_t(c2_map_errno<ENOMEM, EACCES, EINVAL>(ret));
}

C2AllocationBuf::C2AllocationBuf(size_t size, int shareFd, C2Allocator::id_t id)
    : C2LinearAllocation(size), mHandle(-1, 0) {
  mHandle = C2HandleBuf(shareFd, size);
  mId = id;
  mInit = c2_status_t(c2_map_errno<ENOMEM, EACCES, EINVAL>(0));
}

/* =========================== DMABUF ALLOCATOR =========================== */
C2AllocatorBuf::C2AllocatorBuf(id_t id) : mInit(C2_OK) {
  C2MemoryUsage minUsage = {0, 0};
  C2MemoryUsage maxUsage = {C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE};
  Traits traits = {"android.allocator.dmabuf", id, LINEAR, minUsage, maxUsage};
  mTraits = std::make_shared<Traits>(traits);

  int32_t heapmask = ~0;
  int32_t flags = 0;
  uint32_t align = 0;

#ifdef __ANDROID_APEX__
  heapmask = base::GetIntProperty("ro.com.android.media.swcodec.ion.heapmask",
                                  int32_t(0xFFFFFFFF));
  flags = base::GetIntProperty("ro.com.android.media.swcodec.ion.flags", 0);
  align = base::GetUintProperty("ro.com.android.media.swcodec.ion.align", 0u);
#endif

  ALOGD("JDB: Default ION heapmask = %d", heapmask);
  ALOGD("JDB: Default ION flags = %d", flags);
  ALOGD("JDB: Default ION align = %d", align);
  /*
   * So one issue here is we don't have a way to multiplex
   * the ION_FLAG_CACHED onto dmabuf heaps
   */
  mBufferAllocator.MapNameToIonHeap("system", "ion_system_heap",
                                    ION_FLAG_CACHED | flags, heapmask,
                                    ION_FLAG_CACHED | flags);
  mBufferAllocator.MapNameToIonHeap("system-uncached", "ion_system_heap", flags,
                                    heapmask, flags);
}

C2Allocator::id_t C2AllocatorBuf::getId() const {
  std::lock_guard<std::mutex> lock(mUsageMapperLock);
  return mTraits->id;
}

C2String C2AllocatorBuf::getName() const {
  std::lock_guard<std::mutex> lock(mUsageMapperLock);
  return mTraits->name;
}

std::shared_ptr<const C2Allocator::Traits> C2AllocatorBuf::getTraits() const {
  std::lock_guard<std::mutex> lock(mUsageMapperLock);
  return mTraits;
}

void C2AllocatorBuf::setUsageMapper(const UsageMapperFn &mapper __unused,
                                    uint64_t minUsage, uint64_t maxUsage,
                                    uint64_t blockSize __unused) {
  std::lock_guard<std::mutex> lock(mUsageMapperLock);
  Traits traits = {mTraits->name, mTraits->id, LINEAR, C2MemoryUsage(minUsage),
                   C2MemoryUsage(maxUsage)};
  mTraits = std::make_shared<Traits>(traits);
}

c2_status_t C2AllocatorBuf::newLinearAllocation(
    uint32_t capacity, C2MemoryUsage usage,
    std::shared_ptr<C2LinearAllocation> *allocation) {
  if (allocation == nullptr) {
    return C2_BAD_VALUE;
  }

  allocation->reset();
  if (mInit != C2_OK) {
    return mInit;
  }

  size_t align = 0;

  std::shared_ptr<C2AllocationBuf> alloc = std::make_shared<C2AllocationBuf>(
      mBufferAllocator, capacity, align, usage, getId());
  c2_status_t ret = alloc->status();
  if (ret == C2_OK) {
    *allocation = alloc;
  }
  return ret;
}

c2_status_t C2AllocatorBuf::priorLinearAllocation(
    const C2Handle *handle, std::shared_ptr<C2LinearAllocation> *allocation) {
  *allocation = nullptr;
  if (mInit != C2_OK) {
    return mInit;
  }

  if (!C2HandleBuf::isValid(handle)) {
    return C2_BAD_VALUE;
  }

  // TODO: get capacity and validate it
  const C2HandleBuf *h = static_cast<const C2HandleBuf *>(handle);
  std::shared_ptr<C2AllocationBuf> alloc =
      std::make_shared<C2AllocationBuf>(h->size(), h->bufferFd(), getId());
  c2_status_t ret = alloc->status();
  if (ret == C2_OK) {
    *allocation = alloc;
    native_handle_delete(const_cast<native_handle_t *>(
        reinterpret_cast<const native_handle_t *>(handle)));
  }
  return ret;
}

bool C2AllocatorBuf::isValid(const C2Handle *const o) {
  return C2HandleBuf::isValid(o);
}

} // namespace android
