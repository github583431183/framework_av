/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <condition_variable>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/usb/ch9.h>
#include <linux/usb/functionfs.h>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/endian.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "AsyncIO.h"
#include "IUsbHandle.h"

#define cpu_to_le16(x)  htole16(x)
#define cpu_to_le32(x)  htole32(x)

constexpr char ffs_mtp_ep_out[] = "/dev/usb-ffs/mtp/ep1";
constexpr char ffs_mtp_ep_in[] = "/dev/usb-ffs/mtp/ep2";
constexpr char ffs_mtp_ep_intr[] = "/dev/usb-ffs/mtp/ep3";

constexpr int max_packet_size_fs = 64;
constexpr int max_packet_size_hs = 512;
constexpr int max_packet_size_ss = 1024;

// Must be divisible by all max packet size values
constexpr int max_file_chunk_size = 3145728;
constexpr int usb_ffs_max_write = 262144;
constexpr int usb_ffs_max_read = 262144;

constexpr unsigned int max_mtp_file_size = 0xFFFFFFFF;

class UsbFfsHandle : public IUsbHandle {
    private:
    int writeHandle(int fd, const void *data, int len);
    int readHandle(int fd, void *data, int len);
    int spliceReadHandle(int fd, int fd_out, int len);
    bool initFunctionfs();
    void closeConfig();
    void closeEndpoints();

    bool ptp;

    bool ready;
    std::condition_variable ready_notify;
    std::mutex ready_lock;

    int control;
    int bulk_out; /* "out" from the host's perspective => source for mtp server */
    int bulk_in;  /* "in" from the host's perspective => sink for mtp server */
    int intr;

    public:
    int read(void *data, int len);
    int write(const void *data, int len);

    int receiveFile(mtp_file_range mfr);
    int sendFile(mtp_file_range mfr);
    int sendEvent(mtp_event me);

    int start();
    int close();

    int configure(bool ptp);

    UsbFfsHandle();
    ~UsbFfsHandle();
};

/* FunctionFS header objects */

struct mtp_data_header {
    /* length of packet, including this header */
    __le32 length;
    /* container type (2 for data packet) */
    __le16 type;
    /* MTP command code */
    __le16 command;
    /* MTP transaction ID */
    __le32 transaction_id;
};

struct func_desc {
    struct usb_interface_descriptor intf;
    struct usb_endpoint_descriptor_no_audio source;
    struct usb_endpoint_descriptor_no_audio sink;
    struct usb_endpoint_descriptor_no_audio intr;
} __attribute__((packed));

struct ss_func_desc {
    struct usb_interface_descriptor intf;
    struct usb_endpoint_descriptor_no_audio source;
    struct usb_ss_ep_comp_descriptor source_comp;
    struct usb_endpoint_descriptor_no_audio sink;
    struct usb_ss_ep_comp_descriptor sink_comp;
    struct usb_endpoint_descriptor_no_audio intr;
    struct usb_ss_ep_comp_descriptor intr_comp;
} __attribute__((packed));

struct desc_v1 {
    struct usb_functionfs_descs_head_v1 {
        __le32 magic;
        __le32 length;
        __le32 fs_count;
        __le32 hs_count;
    } __attribute__((packed)) header;
    struct func_desc fs_descs, hs_descs;
} __attribute__((packed));

struct desc_v2 {
    struct usb_functionfs_descs_head_v2 header;
    // The rest of the structure depends on the flags in the header.
    __le32 fs_count;
    __le32 hs_count;
    __le32 ss_count;
    __le32 os_count;
    struct func_desc fs_descs, hs_descs;
    struct ss_func_desc ss_descs;
    struct usb_os_desc_header os_header;
    struct usb_ext_compat_desc os_desc;
} __attribute__((packed));

static struct usb_interface_descriptor mtp_interface_desc = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0,
    .bNumEndpoints = 3,
    .bInterfaceClass = USB_CLASS_STILL_IMAGE,
    .bInterfaceSubClass = 1,
    .bInterfaceProtocol = 1,
    .iInterface = 1, /* first string from the provided table */
};

static struct usb_interface_descriptor ptp_interface_desc = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0,
    .bNumEndpoints = 3,
    .bInterfaceClass = USB_CLASS_STILL_IMAGE,
    .bInterfaceSubClass = 1,
    .bInterfaceProtocol = 1,
};

struct usb_endpoint_descriptor_no_audio fs_source = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 1 | USB_DIR_OUT,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = max_packet_size_fs,
};

struct usb_endpoint_descriptor_no_audio fs_sink = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 2 | USB_DIR_IN,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = max_packet_size_fs,
};

struct usb_endpoint_descriptor_no_audio fs_intr = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 3 | USB_DIR_IN,
    .bmAttributes = USB_ENDPOINT_XFER_INT,
    .wMaxPacketSize = max_packet_size_fs,
    .bInterval = 6,
};

struct usb_endpoint_descriptor_no_audio hs_source = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 1 | USB_DIR_OUT,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = max_packet_size_hs,
};

struct usb_endpoint_descriptor_no_audio hs_sink = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 2 | USB_DIR_IN,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = max_packet_size_hs,
};

struct usb_endpoint_descriptor_no_audio hs_intr = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 3 | USB_DIR_IN,
    .bmAttributes = USB_ENDPOINT_XFER_INT,
    .wMaxPacketSize = max_packet_size_hs,
    .bInterval = 6,
};

struct usb_endpoint_descriptor_no_audio ss_source = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 1 | USB_DIR_OUT,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = max_packet_size_ss,
};

struct usb_endpoint_descriptor_no_audio ss_sink = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 2 | USB_DIR_IN,
    .bmAttributes = USB_ENDPOINT_XFER_BULK,
    .wMaxPacketSize = max_packet_size_ss,
};

struct usb_endpoint_descriptor_no_audio ss_intr = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 3 | USB_DIR_IN,
    .bmAttributes = USB_ENDPOINT_XFER_INT,
    .wMaxPacketSize = max_packet_size_ss,
    .bInterval = 6,
};

static usb_ss_ep_comp_descriptor ss_source_comp = {
    .bLength = sizeof(ss_source_comp),
    .bDescriptorType = USB_DT_SS_ENDPOINT_COMP,
    .bMaxBurst = 2,
};

static usb_ss_ep_comp_descriptor ss_sink_comp = {
    .bLength = sizeof(ss_sink_comp),
    .bDescriptorType = USB_DT_SS_ENDPOINT_COMP,
    .bMaxBurst = 2,
};

static usb_ss_ep_comp_descriptor ss_intr_comp = {
    .bLength = sizeof(ss_intr_comp),
    .bDescriptorType = USB_DT_SS_ENDPOINT_COMP,
//    .wBytesPerInterval = cpu_to_le16(64)
};

static struct func_desc mtp_fs_descriptors = {
    .intf = mtp_interface_desc,
    .source = fs_source,
    .sink = fs_sink,
    .intr = fs_intr,
};

static struct func_desc mtp_hs_descriptors = {
    .intf = mtp_interface_desc,
    .source = hs_source,
    .sink = hs_sink,
    .intr = hs_intr,
};

static struct ss_func_desc mtp_ss_descriptors = {
    .intf = mtp_interface_desc,
    .source = ss_source,
    .source_comp = ss_source_comp,
    .sink = ss_sink,
    .sink_comp = ss_sink_comp,
    .intr = ss_intr,
    .intr_comp = ss_intr_comp,
};

static struct func_desc ptp_fs_descriptors = {
    .intf = ptp_interface_desc,
    .source = fs_source,
    .sink = fs_sink,
    .intr = fs_intr,
};

static struct func_desc ptp_hs_descriptors = {
    .intf = ptp_interface_desc,
    .source = hs_source,
    .sink = hs_sink,
    .intr = hs_intr,
};

static struct ss_func_desc ptp_ss_descriptors = {
    .intf = ptp_interface_desc,
    .source = ss_source,
    .source_comp = ss_source_comp,
    .sink = ss_sink,
    .sink_comp = ss_sink_comp,
    .intr = ss_intr,
    .intr_comp = ss_intr_comp,
};

struct usb_ext_compat_desc os_desc_compat = {
    .bFirstInterfaceNumber = 1,
    .Reserved1 = 0,
    .CompatibleID = {0},
    .SubCompatibleID = {0},
    .Reserved2 = {0},
};

static struct usb_os_desc_header os_desc_header = {
    .interface = 1,
    .dwLength = cpu_to_le32(sizeof(os_desc_header) + sizeof(os_desc_compat)),
    .bcdVersion = cpu_to_le16(1),
    .wIndex = cpu_to_le16(4),
    .bCount = 1,
    .Reserved = 0,
};

#define STR_INTERFACE_ "MTP"

static const struct {
    struct usb_functionfs_strings_head header;
    struct {
        __le16 code;
        const char str1[sizeof(STR_INTERFACE_)];
    } __attribute__((packed)) lang0;
} __attribute__((packed)) strings = {
    .header = {
        .magic = cpu_to_le32(FUNCTIONFS_STRINGS_MAGIC),
        .length = cpu_to_le32(sizeof(strings)),
        .str_count = cpu_to_le32(1),
        .lang_count = cpu_to_le32(1),
    },
    .lang0 = {
        cpu_to_le16(0x0409), /* en-us */
        STR_INTERFACE_,
    },
};

UsbFfsHandle::UsbFfsHandle()
    :   ready(false),
        control(-1),
        bulk_out(-1),
        bulk_in(-1),
        intr(-1)
{
}

UsbFfsHandle::~UsbFfsHandle() {}

void UsbFfsHandle::closeEndpoints() {
    if (intr > 0) {
        ::close(intr);
        intr = -1;
    }
    if (bulk_in > 0) {
        ::close(bulk_in);
        bulk_in = -1;
    }
    if (bulk_out > 0) {
        ::close(bulk_out);
        bulk_out = -1;
    }
}

bool UsbFfsHandle::initFunctionfs()
{
    ssize_t ret;
    struct desc_v1 v1_descriptor;
    struct desc_v2 v2_descriptor;

    v2_descriptor.header.magic = cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2);
    v2_descriptor.header.length = cpu_to_le32(sizeof(v2_descriptor));
    v2_descriptor.header.flags = FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC |
                                 FUNCTIONFS_HAS_SS_DESC | FUNCTIONFS_HAS_MS_OS_DESC;
    v2_descriptor.fs_count = 4;
    v2_descriptor.hs_count = 4;
    v2_descriptor.ss_count = 7;
    v2_descriptor.os_count = 1;
    v2_descriptor.fs_descs = ptp ? ptp_fs_descriptors : mtp_fs_descriptors;
    v2_descriptor.hs_descs = ptp ? ptp_hs_descriptors : mtp_hs_descriptors;
    v2_descriptor.ss_descs = ptp ? ptp_ss_descriptors : mtp_ss_descriptors;
    v2_descriptor.os_header = os_desc_header;
    v2_descriptor.os_desc = os_desc_compat;

    if (control < 0) { // might have already done this before
        control = TEMP_FAILURE_RETRY(open(ffs_mtp_ep0, O_RDWR));
        if (control < 0) {
            PLOG(ERROR) << ffs_mtp_ep0 << ": cannot open control endpoint";
            goto err;
        }

        ret = TEMP_FAILURE_RETRY(::write(control, &v2_descriptor, sizeof(v2_descriptor)));
        if (ret < 0) {
            v1_descriptor.header.magic = cpu_to_le32(FUNCTIONFS_DESCRIPTORS_MAGIC);
            v1_descriptor.header.length = cpu_to_le32(sizeof(v1_descriptor));
            v1_descriptor.header.fs_count = 4;
            v1_descriptor.header.hs_count = 4;
            v1_descriptor.fs_descs = ptp ? ptp_fs_descriptors : mtp_fs_descriptors;
            v1_descriptor.hs_descs = ptp ? ptp_hs_descriptors : mtp_hs_descriptors;
            PLOG(ERROR) << ffs_mtp_ep0 << "Switching to V1 descriptor format";
            ret = TEMP_FAILURE_RETRY(::write(control, &v1_descriptor, sizeof(v1_descriptor)));
            if (ret < 0) {
                PLOG(ERROR) << ffs_mtp_ep0 << "Writing descriptors failed";
                goto err;
            }
        }
        ret = TEMP_FAILURE_RETRY(::write(control, &strings, sizeof(strings)));
        if (ret < 0) {
            PLOG(ERROR) << ffs_mtp_ep0 << "Writing strings failed";
            goto err;
        }
    }

    bulk_out = TEMP_FAILURE_RETRY(open(ffs_mtp_ep_out, O_RDWR));
    if (bulk_out < 0) {
        PLOG(ERROR) << ffs_mtp_ep_out << ": cannot open bulk out ep";
        goto err;
    }

    bulk_in = TEMP_FAILURE_RETRY(open(ffs_mtp_ep_in, O_RDWR));
    if (bulk_in < 0) {
        PLOG(ERROR) << ffs_mtp_ep_in << ": cannot open bulk in ep";
        goto err;
    }

    intr = TEMP_FAILURE_RETRY(open(ffs_mtp_ep_intr, O_RDWR));
    if (intr < 0) {
        PLOG(ERROR) << ffs_mtp_ep0 << ": cannot open intr ep";
        goto err;
    }

    android::base::SetProperty("sys.usb.ffs.ready", "1");
    return true;

err:
    closeEndpoints();
    closeConfig();
    return false;
}

void UsbFfsHandle::closeConfig() {
    if (control > 0) {
        ::close(control);
        control = -1;
    }
}

int UsbFfsHandle::writeHandle(int fd, const void* data, int len) {
    PLOG(VERBOSE) << "MTP about to write fd = " << fd << ", len=" << len;
    int ret = 0;
    const char* buf = static_cast<const char*>(data);
    while (len > 0) {
        int write_len = std::min(usb_ffs_max_write, len);
        int n = TEMP_FAILURE_RETRY(::write(fd, buf, write_len));

        if (n < 0) {
            PLOG(ERROR) << "write ERROR: fd = " << fd << ", n = " << n;
            return -1;
        } else if (n < write_len) {
            PLOG(ERROR) << "less written than expected";
            return -1;
        }
        buf += n;
        len -= n;
        ret += n;
    }
    return ret;
}

int UsbFfsHandle::write(const void* data, int len) {
    int ret = writeHandle(bulk_in, data, len);
    return ret;
}

int UsbFfsHandle::readHandle(int fd, void* data, int len) {
    PLOG(VERBOSE) << "MTP about to read fd = " << fd << ", len=" << len;
    int ret = 0;
    char* buf = static_cast<char*>(data);
    while (len > 0) {
        int read_len = std::min(usb_ffs_max_read, len);
        int n = TEMP_FAILURE_RETRY(::read(fd, buf, read_len));
        if (n < 0) {
            PLOG(ERROR) << "read ERROR: fd = " << fd << ", n = " << n;
            return -1;
        }
        ret += n;
        if (n < read_len) // done reading early
            break;
        buf += n;
        len -= n;
    }
    return ret;
}

int UsbFfsHandle::spliceReadHandle(int fd, int pipe_out, int len) {
    PLOG(VERBOSE) << "MTP about to splice read fd = " << fd << ", len=" << len;
    int ret = 0;
    loff_t dummyoff;
    while (len > 0) {
        int read_len = std::min(usb_ffs_max_read, len);
        dummyoff = 0;
        int n = TEMP_FAILURE_RETRY(splice(fd, &dummyoff, pipe_out, nullptr, read_len, 0));
        if (n < 0) {
            PLOG(ERROR) << "splice read ERROR: fd = " << fd << ", n = " << n;
            return -1;
        }
        ret += n;
        if (n < read_len) // done reading early
            break;
        len -= n;
    }
    return ret;
}

int UsbFfsHandle::read(void* data, int len) {
    int ret = readHandle(bulk_out, data, len);
    return ret;
}

int UsbFfsHandle::close() {
    closeEndpoints();
    return 0;
}

int UsbFfsHandle::start() {
    int ret = 0;
    std::unique_lock<std::mutex> lk(ready_lock);

    // Wait till configuration is complete
    ready_notify.wait(lk, [this](){return ready;});
    ready = false;

    lk.unlock();
    return ret;
}

int UsbFfsHandle::configure(bool usePtp) {
    // Don't do anything if ffs is already open
    if (bulk_in > 0) return 0;

    // If ptp is changed, the configuration must be rewritten
    if (ptp != usePtp) closeConfig();
    ptp = usePtp;

    if (!initFunctionfs()) {
        return -1;
    }
    // tell server that descriptors are finished
    {
        std::lock_guard<std::mutex> lk(ready_lock);
        ready = true;
    }
    ready_notify.notify_one();

    return 0;
}

/* Read from USB and write to a local file. */
int UsbFfsHandle::receiveFile(mtp_file_range mfr) {
    // When receiving files, the incoming length is given in 32 bits.
    // A >4G file is given as 0xFFFFFFFF
    uint32_t file_length = mfr.length;
    uint64_t offset = lseek(mfr.fd, 0, SEEK_CUR);

    int buf1_len = std::min((uint32_t) max_file_chunk_size, file_length);
    std::vector<char> buf1(buf1_len);
    char* data = buf1.data();

    // If necessary, allocate a second buffer for background r/w
    int buf2_len = std::min((uint32_t) max_file_chunk_size,
            file_length - max_file_chunk_size);
    std::vector<char> buf2(buf2_len);
    char *data2 = buf2.data();

    struct aiocb aio;
    aio.aio_fildes = mfr.fd;
    aio.aio_buf = nullptr;
    const struct aiocb * const aiol[] = {&aio};
    int ret;

    posix_fadvise(mfr.fd, 0, 0, POSIX_FADV_SEQUENTIAL | POSIX_FADV_NOREUSE);

    // Break down the file into pieces that fit in buffers
    while (file_length > 0) {
        size_t length = std::min((uint32_t) max_file_chunk_size, file_length);

        // Read data from USB
        if ((ret = readHandle(bulk_out, data, length)) == -1) {
            return -1;
        }

        if (file_length != max_mtp_file_size && ret < (int) length) {
            errno = EIO;
            return -1;
        }

        if (aio.aio_buf) {
            // If this isn't the first time through the loop,
            // get the return status of the last write request
            aio_suspend(aiol, 1, nullptr);

            int written = aio_return(&aio);
            if (written == -1) {
                errno = aio_error(&aio);
                return -1;
            }
            if ((size_t) written < aio.aio_nbytes) {
                errno = EIO;
                return -1;
            }
        }

        // Enqueue a new write request
        aio.aio_buf = data;
        aio.aio_sink = mfr.fd;
        aio.aio_offset = offset;
        aio.aio_nbytes = ret;
        aio_write(&aio);

        if (file_length == max_mtp_file_size) {
            // For larger files, receive until a short packet is received.
            if ((size_t) ret < length) {
                break;
            }
        }

        if (file_length != max_mtp_file_size) file_length -= ret;
        offset += ret;
        std::swap(data, data2);
    }
    // Wait for the final write to finish
    aio_suspend(aiol, 1, nullptr);
    ret = aio_return(&aio);
    if (ret == -1) {
        errno = aio_error(&aio);
        return -1;
    }
    if ((size_t) ret < aio.aio_nbytes) {
        errno = EIO;
        return -1;
    };

    return 0;
}

/* Read from a local file and send over USB. */
int UsbFfsHandle::sendFile(mtp_file_range mfr) {
    uint64_t file_length = mfr.length;
    uint32_t given_length = std::min((uint64_t) max_mtp_file_size,
            file_length + sizeof(mtp_data_header));
    uint64_t offset = 0;
    struct usb_endpoint_descriptor bulk_in_desc;
    int packet_size;

    if (ioctl(bulk_in, FUNCTIONFS_ENDPOINT_DESC, (unsigned long) &bulk_in_desc)) {
        PLOG(ERROR) << "Could not get FFS bulk-in descriptor: " << strerror(errno);
        return -1;
    }
    packet_size = bulk_in_desc.wMaxPacketSize;

    posix_fadvise(mfr.fd, 0, 0, POSIX_FADV_SEQUENTIAL | POSIX_FADV_NOREUSE);

    int init_read_len = packet_size - sizeof(mtp_data_header);
    int buf1_len = std::max((uint64_t) packet_size, std::min(
                  (uint64_t) max_file_chunk_size, file_length - init_read_len));
    std::vector<char> buf1(buf1_len);
    char *data = buf1.data();

    // If necessary, allocate a second buffer for background r/w
    int buf2_len = std::min((uint64_t) max_file_chunk_size,
            file_length - max_file_chunk_size - init_read_len);
    std::vector<char>buf2(buf2_len);
    char *data2 = buf2.data();

    struct aiocb aio;
    aio.aio_fildes = mfr.fd;
    const struct aiocb * const aiol[] = {&aio};
    int ret, length;

    // Send the header data
    mtp_data_header *header = reinterpret_cast<mtp_data_header*>(data);
    header->length = __cpu_to_le32(given_length);
    header->type = __cpu_to_le16(2); /* data packet */
    header->command = __cpu_to_le16(mfr.command);
    header->transaction_id = __cpu_to_le32(mfr.transaction_id);

    // Windows doesn't support header/data separation even though MTP allows it
    // Handle by filling first packet with initial file data
    if (TEMP_FAILURE_RETRY(pread(mfr.fd, reinterpret_cast<char*>(data) +
                    sizeof(mtp_data_header), init_read_len, offset))
            != init_read_len) return -1;
    file_length -= init_read_len;
    offset += init_read_len;
    if (writeHandle(bulk_in, data, packet_size) == -1) return -1;
    if (file_length == 0) return 0;

    length = std::min((uint64_t) max_file_chunk_size, file_length);
    // Queue up the first read
    aio.aio_buf = data;
    aio.aio_offset = offset;
    aio.aio_nbytes = length;
    aio_read(&aio);

    // Break down the file into pieces that fit in buffers
    while(file_length > 0) {
        // Wait for the previous read to finish
        aio_suspend(aiol, 1, nullptr);
        ret = aio_return(&aio);
        if (ret == -1) {
            errno = aio_error(&aio);
            return -1;
        }
        if ((size_t) ret < aio.aio_nbytes) {
            errno = EIO;
            return -1;
        }

        file_length -= ret;
        offset += ret;
        std::swap(data, data2);

        if (file_length > 0) {
            length = std::min((uint64_t) max_file_chunk_size, file_length);
            // Queue up another read
            aio.aio_buf = data;
            aio.aio_offset = offset;
            aio.aio_nbytes = length;
            aio_read(&aio);
        }

        if (writeHandle(bulk_in, data2, ret) == -1) return -1;
    }

    if (given_length == max_mtp_file_size && ret % packet_size == 0) {
        // If the last packet wasn't short, send a final empty packet
        if (writeHandle(bulk_in, data, 0) == -1) return -1;
    }

    return 0;
}

int UsbFfsHandle::sendEvent(mtp_event me) {
    unsigned length = me.length;
    int ret = writeHandle(intr, me.data, length);
    return ret;
}

IUsbHandle *get_ffs_handle() {
    return new UsbFfsHandle();
}

