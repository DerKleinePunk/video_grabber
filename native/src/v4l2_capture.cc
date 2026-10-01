#include "v4l2_capture.h"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace vg {

namespace {

class Posix final : public Sys {
 public:
  int Open(const char* path, int flags) override { return ::open(path, flags); }
  int Close(int fd) override { return ::close(fd); }
  int Ioctl(int fd, unsigned long request, void* arg) override {
    int r;
    do {
      r = ::ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
  }
  void* Mmap(size_t length, int fd, long offset) override {
    return ::mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                  offset);
  }
  int Munmap(void* addr, size_t length) override {
    return ::munmap(addr, length);
  }
  int PollIn(int fd, int timeout_ms, short* revents) override {
    pollfd p{fd, POLLIN, 0};
    int r;
    do {
      r = ::poll(&p, 1, timeout_ms);
    } while (r == -1 && errno == EINTR);
    *revents = r > 0 ? p.revents : 0;
    return r;
  }
  int Errno() const override { return errno; }
};

bool IsGone(int err) { return err == ENODEV || err == ENXIO || err == EIO; }

}  // namespace

std::unique_ptr<Sys> RealSys() { return std::make_unique<Posix>(); }

V4l2Capture::V4l2Capture(std::unique_ptr<Sys> sys) : sys_(std::move(sys)) {}

V4l2Capture::~V4l2Capture() { Close(); }

OpenResult V4l2Capture::Fail(const char* what) {
  last_error_ = std::string(what) + ": " + std::strerror(sys_->Errno());
  Close();
  return OpenResult::kError;
}

OpenResult V4l2Capture::Open(const CaptureConfig& config) {
  Close();
  last_error_.clear();
  fd_ = sys_->Open(config.device.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
  if (fd_ < 0) {
    const int err = sys_->Errno();
    last_error_ = config.device + ": " + std::strerror(err);
    fd_ = -1;
    return (err == ENOENT || IsGone(err)) ? OpenResult::kMissing
                                          : OpenResult::kError;
  }

  int input = static_cast<int>(config.input);
  if (sys_->Ioctl(fd_, VIDIOC_S_INPUT, &input) < 0) {
    return Fail("VIDIOC_S_INPUT");
  }
  v4l2_std_id std = config.pal ? V4L2_STD_PAL : V4L2_STD_NTSC;
  if (sys_->Ioctl(fd_, VIDIOC_S_STD, &std) < 0) {
    return Fail("VIDIOC_S_STD");
  }

  v4l2_format fmt{};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = 720;
  fmt.fmt.pix.height = config.pal ? 576 : 480;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
  fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
  if (sys_->Ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
    return Fail("VIDIOC_S_FMT");
  }
  if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_UYVY) {
    last_error_ = "Grabber liefert kein UYVY";
    Close();
    return OpenResult::kError;
  }
  width_ = fmt.fmt.pix.width;
  height_ = fmt.fmt.pix.height;
  stride_ = fmt.fmt.pix.bytesperline != 0 ? fmt.fmt.pix.bytesperline
                                          : size_t{width_} * 2;

  v4l2_requestbuffers req{};
  req.count = config.buffers;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  if (sys_->Ioctl(fd_, VIDIOC_REQBUFS, &req) < 0 || req.count == 0) {
    return Fail("VIDIOC_REQBUFS");
  }
  for (uint32_t i = 0; i < req.count; ++i) {
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;
    if (sys_->Ioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
      return Fail("VIDIOC_QUERYBUF");
    }
    void* addr = sys_->Mmap(buf.length, fd_, buf.m.offset);
    if (addr == MAP_FAILED) {
      return Fail("mmap");
    }
    mappings_.push_back({addr, buf.length});
    if (sys_->Ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
      return Fail("VIDIOC_QBUF");
    }
  }
  int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (sys_->Ioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
    return Fail("VIDIOC_STREAMON");
  }
  streaming_ = true;
  return OpenResult::kOk;
}

WaitResult V4l2Capture::Wait(int timeout_ms, Frame* frame) {
  if (fd_ < 0) {
    return WaitResult::kGone;
  }
  short revents = 0;
  const int r = sys_->PollIn(fd_, timeout_ms, &revents);
  if (r < 0) {
    return IsGone(sys_->Errno()) ? WaitResult::kGone : WaitResult::kError;
  }
  if (r == 0) {
    return WaitResult::kTimeout;
  }
  if ((revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
    return WaitResult::kGone;
  }
  v4l2_buffer buf{};
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  if (sys_->Ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
    const int err = sys_->Errno();
    if (err == EAGAIN) {
      return WaitResult::kTimeout;
    }
    last_error_ = std::string("VIDIOC_DQBUF: ") + std::strerror(err);
    return IsGone(err) ? WaitResult::kGone : WaitResult::kError;
  }
  if (buf.index >= mappings_.size()) {
    last_error_ = "VIDIOC_DQBUF: unbekannter Puffer";
    return WaitResult::kError;
  }
  frame->data = static_cast<const uint8_t*>(mappings_[buf.index].addr);
  frame->bytes = buf.bytesused;
  frame->width = width_;
  frame->height = height_;
  frame->stride = stride_;
  frame->index = buf.index;
  frame->timestamp_us = static_cast<uint64_t>(buf.timestamp.tv_sec) * 1000000u +
                        static_cast<uint64_t>(buf.timestamp.tv_usec);
  return WaitResult::kFrame;
}

void V4l2Capture::Release(const Frame& frame) {
  if (fd_ < 0) {
    return;
  }
  v4l2_buffer buf{};
  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.index = frame.index;
  sys_->Ioctl(fd_, VIDIOC_QBUF, &buf);
}

void V4l2Capture::Close() {
  if (fd_ >= 0 && streaming_) {
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sys_->Ioctl(fd_, VIDIOC_STREAMOFF, &type);
  }
  streaming_ = false;
  for (const auto& m : mappings_) {
    sys_->Munmap(m.addr, m.length);
  }
  mappings_.clear();
  if (fd_ >= 0) {
    sys_->Close(fd_);
    fd_ = -1;
  }
}

}  // namespace vg
