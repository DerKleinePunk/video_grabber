// Platform view for ivi-homescreen (ihs_shared, docs/PLUGIN_ABI.md): shows the
// grabber picture without GStreamer. One thread per view reads V4L2, repacks
// the top field to NV16 into a ring of DRM dumb buffers and hands it on with
// ihs_pv_submit. The shell puts NV16 on a KMS plane if one is free, otherwise
// it draws it as a texture.
//
// Dart loads the library, calls vg_register() and polls the state with
// vg_status(view_id).

#include <drm/drm.h>
#include <drm/drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <thread>

#include "ihs/platform_view.h"
#include "params.h"
#include "status.h"
#include "uyvy.h"
#include "v4l2_capture.h"

namespace vg {
namespace {

constexpr const char* kViewType = "video_grabber/view";
constexpr int kRingSize = 3;

uint64_t NowMs() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

// State per view id for vg_status(); the view itself lives in the registry
// handle.
std::mutex g_status_mutex;
std::map<int32_t, Status> g_status;

void PublishStatus(int32_t id, Status s) {
  std::scoped_lock const lock(g_status_mutex);
  g_status[id] = s;
}

void DropStatus(int32_t id) {
  std::scoped_lock const lock(g_status_mutex);
  g_status.erase(id);
}

// One NV16 buffer: Y lines, below them the CbCr lines, same pitch.
struct DumbBuffer {
  int fd = -1;  // PRIME dma-buf, owned here; ihs_pv_submit gets dups
  uint8_t* map = nullptr;
  size_t size = 0;
  uint32_t pitch = 0;
  int release_fence = -1;
};

bool AllocNv16(int drm_fd, uint32_t width, uint32_t height, DumbBuffer* out) {
  drm_mode_create_dumb creq{};
  creq.width = width;
  // Y + CbCr, height lines each; one tile of slack, see the IhsFrame note.
  creq.height = height * 2 + 64;
  creq.bpp = 8;
  if (::ioctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) != 0) {
    return false;
  }
  drm_mode_map_dumb mreq{};
  mreq.handle = creq.handle;
  void* m = MAP_FAILED;
  if (::ioctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) == 0) {
    m = ::mmap(nullptr, creq.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd,
               static_cast<off_t>(mreq.offset));
  }
  drm_prime_handle ph{};
  ph.handle = creq.handle;
  ph.flags = O_CLOEXEC | O_RDWR;
  const bool exported =
      ::ioctl(drm_fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &ph) == 0 && ph.fd >= 0;
  // The dma-buf and the mapping keep the memory alive, the GEM handle can go.
  drm_mode_destroy_dumb dreq{};
  dreq.handle = creq.handle;
  ::ioctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
  if (m == MAP_FAILED || !exported) {
    if (m != MAP_FAILED) {
      ::munmap(m, creq.size);
    }
    if (exported) {
      ::close(ph.fd);
    }
    return false;
  }
  out->fd = ph.fd;
  out->map = static_cast<uint8_t*>(m);
  out->size = creq.size;
  out->pitch = creq.pitch;
  return true;
}

void FreeBuffer(DumbBuffer* b) {
  if (b->release_fence >= 0) {
    ::close(b->release_fence);
  }
  if (b->map != nullptr) {
    ::munmap(b->map, b->size);
  }
  if (b->fd >= 0) {
    ::close(b->fd);
  }
  *b = DumbBuffer{};
}

// Waits until the shell releases the buffer (at most @timeout_ms).
void WaitRelease(DumbBuffer* b, int timeout_ms) {
  if (b->release_fence < 0) {
    return;
  }
  pollfd p{b->release_fence, POLLIN, 0};
  ::poll(&p, 1, timeout_ms);
  ::close(b->release_fence);
  b->release_fence = -1;
}

class GrabberView {
 public:
  GrabberView(int32_t id, IhsPlatformView* view, int drm_fd,
              CaptureConfig config)
      : id_(id), view_(view), drm_fd_(drm_fd), config_(std::move(config)) {}

  ~GrabberView() { Stop(); }

  void Start() {
    PublishStatus(id_, Status::kConnecting);
    thread_ = std::thread([this] { Run(); });
  }

  // After return no ihs_pv_submit runs any more (required by platform_view.h).
  void Stop() {
    stop_.store(true);
    if (thread_.joinable()) {
      thread_.join();
      std::fprintf(stderr,
                   "[video_grabber] view %d: %llu incomplete frames dropped\n",
                   id_, static_cast<unsigned long long>(incomplete_));
    }
    for (auto& b : ring_) {
      FreeBuffer(&b);
    }
  }

  void SetSuspended(bool suspended) { suspended_.store(suspended); }
  [[nodiscard]] int32_t id() const { return id_; }

 private:
  void Run() {
    V4l2Capture cap(RealSys());
    StatusTracker tracker;
    Status shown = Status::kConnecting;
    auto publish = [&] {
      if (tracker.status() != shown) {
        shown = tracker.status();
        PublishStatus(id_, shown);
        std::fprintf(stderr, "[video_grabber] view %d: status %d %s\n", id_,
                     static_cast<int>(shown), cap.last_error().c_str());
      }
    };

    while (!stop_.load()) {
      if (!cap.is_open()) {
        tracker.OnOpen(cap.Open(config_), NowMs());
        publish();
        if (!cap.is_open()) {
          Sleep(1000);
          continue;
        }
        field_ = cap.is_usb_camera() ? Field::kBoth : Field::kTop;
        if (!EnsureRing(cap.width(), OutputHeight(cap.height(), field_))) {
          std::fprintf(stderr, "[video_grabber] view %d: no dumb buffers\n",
                       id_);
          PublishStatus(id_, Status::kError);
          return;
        }
        std::fprintf(stderr, "[video_grabber] view %d: %s %ux%u -> %ux%u\n",
                     id_,
                     cap.is_usb_camera() ? "USB camera YUYV" : "grabber UYVY",
                     cap.width(), cap.height(), ring_width_, ring_height_);
      }
      Frame f;
      const WaitResult r = cap.Wait(200, &f);
      tracker.OnWait(r, NowMs());
      publish();
      switch (r) {
        case WaitResult::kFrame:
          if (!f.complete()) {
            ++incomplete_;
          } else if (!suspended_.load()) {
            Submit(f);
          }
          cap.Release(f);
          break;
        case WaitResult::kTimeout:
          break;
        case WaitResult::kGone:
        case WaitResult::kError:
          cap.Close();
          Sleep(500);
          break;
      }
    }
  }

  void Sleep(int ms) {
    for (int waited = 0; waited < ms && !stop_.load(); waited += 50) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }

  bool EnsureRing(uint32_t width, uint32_t height) {
    if (ring_width_ == width && ring_height_ == height) {
      return true;
    }
    for (auto& b : ring_) {
      FreeBuffer(&b);
    }
    for (auto& b : ring_) {
      if (!AllocNv16(drm_fd_, width, height, &b)) {
        return false;
      }
    }
    ring_width_ = width;
    ring_height_ = height;
    return true;
  }

  void Submit(const Frame& f) {
    DumbBuffer& b = ring_[next_];
    WaitRelease(&b, 40);
    const uint32_t h = ring_height_;
    if (!UyvyToNv16(f.data, f.stride, f.width, f.height, field_, b.map, b.pitch,
                    b.map + size_t{b.pitch} * h, b.pitch, f.packing)) {
      return;
    }
    const int fd = ::dup(b.fd);
    if (fd < 0) {
      return;
    }
    IhsFrame frame{};
    frame.struct_size = sizeof(frame);
    frame.format.fourcc = DRM_FORMAT_NV16;
    frame.format.modifier = DRM_FORMAT_MOD_LINEAR;
    frame.color_space = IHS_COLOR_SPACE_BT601;  // SD video
    frame.color_range = IHS_COLOR_RANGE_LIMITED;
    frame.width = f.width;
    frame.height = h;
    frame.plane_count = 2;
    frame.plane_fd[0] = fd;
    frame.plane_fd[1] = fd;
    frame.plane_offset[0] = 0;
    frame.plane_offset[1] = b.pitch * h;
    frame.plane_stride[0] = b.pitch;
    frame.plane_stride[1] = b.pitch;
    frame.buffer_id = static_cast<uint32_t>(next_);
    int release = -1;
    const int rc = ihs_pv_submit(view_, &frame, -1, &release);
    if (rc == IHS_PV_OK && release >= 0) {
      b.release_fence = release;
    } else if (release >= 0) {
      ::close(release);
    }
    if (rc != IHS_PV_OK && !submit_error_logged_) {
      std::fprintf(stderr, "[video_grabber] view %d: submit failed %d\n", id_,
                   rc);
      submit_error_logged_ = true;
    }
    next_ = (next_ + 1) % kRingSize;
  }

  const int32_t id_;
  IhsPlatformView* const view_;
  const int drm_fd_;
  const CaptureConfig config_;
  std::thread thread_;
  std::atomic<bool> stop_{false};
  std::atomic<bool> suspended_{false};
  DumbBuffer ring_[kRingSize];
  uint32_t ring_width_ = 0;
  uint32_t ring_height_ = 0;
  int next_ = 0;
  Field field_ = Field::kTop;
  bool submit_error_logged_ = false;
  uint64_t incomplete_ = 0;
};

void OnDispose(void* user_data) {
  auto* v = static_cast<GrabberView*>(user_data);
  const int32_t id = v->id();
  delete v;  // Stop() joins the thread
  DropStatus(id);
}

void OnSuspended(void* user_data, uint8_t suspended) {
  static_cast<GrabberView*>(user_data)->SetSuspended(suspended != 0);
}

// Settings come from Dart (creationParams). The environment overrides them
// only for manual tests. getenv() runs on the platform thread while the view
// is created; nothing in the process calls setenv(), so it is safe here.
CaptureConfig ConfigFor(const IhsPvCreateInfo* info) {
  CaptureConfig c = DefaultConfig();
  if (!ApplyParams(info->params, info->params_size, &c)) {
    std::fprintf(stderr, "[video_grabber] view %d: invalid params ignored\n",
                 info->id);
  }
  if (const char* d = std::getenv(  // NOLINT(concurrency-mt-unsafe)
          "VG_DEVICE")) {
    c.device = d;
  }
  if (const char* i = std::getenv(  // NOLINT(concurrency-mt-unsafe)
          "VG_INPUT")) {
    if (!ParseUint(i, &c.input)) {
      std::fprintf(stderr, "[video_grabber] VG_INPUT=%s ignored\n", i);
    }
  }
  if (const char* n = std::getenv(  // NOLINT(concurrency-mt-unsafe)
          "VG_NORM")) {
    c.pal = std::string(n) == "pal";
  }
  if (const char* w = std::getenv(  // NOLINT(concurrency-mt-unsafe)
          "VG_WIDTH")) {
    c.width = std::string(w) == "720" ? 720 : 360;
  }
  std::fprintf(stderr, "[video_grabber] view %d: %s input %u %s width %u\n",
               info->id, c.device.c_str(), c.input, c.pal ? "PAL" : "NTSC",
               c.width);
  return c;
}

int Factory(const IhsPvCreateInfo* info, void* /*factory_user_data*/,
            IhsPlatformView* view, IhsPvCallbacks* out_callbacks,
            void** out_user_data) {
  IhsEglContext egl{};
  egl.struct_size = sizeof(egl);
  if (ihs_pv_egl_context(&egl) != IHS_PV_OK || egl.gbm_device == nullptr) {
    std::fprintf(stderr,
                 "[video_grabber] need the drm-kms-egl backend (gbm device)\n");
    return IHS_PV_ERR_UNSUPPORTED;
  }
  const int drm_fd =
      gbm_device_get_fd(static_cast<gbm_device*>(egl.gbm_device));

  static const IhsFormatModifier kNv16{DRM_FORMAT_NV16, 0,
                                       DRM_FORMAT_MOD_LINEAR};
  IhsPvRequirements req{};
  req.struct_size = sizeof(req);
  req.kinds = IHS_PV_KIND_TEXTURE_DMABUF_IMPORT | IHS_PV_KIND_DRM_PLANE |
              IHS_PV_KIND_SOFTWARE_SHM;
  req.formats = &kNv16;
  req.format_count = 1;
  req.needs_alpha = 0;
  req.sync = IHS_PV_SYNC_IMPLICIT;
  req.z_order = IHS_PV_Z_INLINE;
  IhsPvGrant grant{};
  grant.struct_size = sizeof(grant);
  const int rc = ihs_pv_negotiate(view, &req, &grant);
  std::fprintf(stderr,
               "[video_grabber] view %d %.0fx%.0f: negotiate %d kind %u "
               "fourcc %#x\n",
               info->id, info->width, info->height, rc, grant.granted_kind,
               grant.format.fourcc);
  if (rc != IHS_PV_OK || grant.granted_kind == IHS_PV_KIND_SOFTWARE_SHM) {
    // The software path is not wired up under drm-kms-egl.
    return IHS_PV_ERR_UNSUPPORTED;
  }

  auto* v = new GrabberView(info->id, view, drm_fd, ConfigFor(info));
  out_callbacks->struct_size = sizeof(*out_callbacks);
  out_callbacks->dispose = &OnDispose;
  out_callbacks->set_suspended = &OnSuspended;
  *out_user_data = v;
  v->Start();
  return IHS_PV_OK;
}

}  // namespace
}  // namespace vg

extern "C" {

// Registers the view type. Must run on the platform thread; like pv_bench we
// assume Dart runs there and post otherwise.
__attribute__((visibility("default"))) int vg_register() {
  auto do_register = [](void*) {
    const int rc =
        ihs_pv_register_factory(vg::kViewType, &vg::Factory, nullptr);
    std::fprintf(stderr, "[video_grabber] register %s: %d\n", vg::kViewType,
                 rc);
  };
  if (ihs_pv_is_platform_thread != nullptr &&
      (ihs_pv_post_platform_task != nullptr) &&
      ihs_pv_is_platform_thread() == 0) {
    return ihs_pv_post_platform_task(do_register, nullptr);
  }
  do_register(nullptr);
  return IHS_PV_OK;
}

// State of view @view_id (vg::Status), -1 if it does not exist.
__attribute__((visibility("default"))) int32_t vg_status(int32_t view_id) {
  std::scoped_lock const lock(vg::g_status_mutex);
  const auto it = vg::g_status.find(view_id);
  return it == vg::g_status.end() ? -1 : static_cast<int32_t>(it->second);
}

}  // extern "C"
