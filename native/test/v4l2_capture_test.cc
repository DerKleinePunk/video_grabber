#include "../src/v4l2_capture.h"

#include <cerrno>
#include <map>
#include <vector>

#include <linux/videodev2.h>
#include <poll.h>
#include <sys/mman.h>

#include "check.h"

namespace {

// Ein nachgebildeter STK1160: merkt sich, was eingestellt wird, und liefert
// Bilder nur, wenn der Test welche bereitlegt.
struct FakeState {
  int open_errno = 0;            // != 0: Open schlägt fehl
  unsigned long fail_ioctl = 0;  // dieser ioctl schlägt mit fail_errno fehl
  int fail_errno = EINVAL;
  int input = -1;
  v4l2_std_id std = 0;
  uint32_t width = 0, height = 0;
  bool streaming = false;
  bool closed = false;
  int munmaps = 0;
  std::vector<uint32_t> queued;   // Puffer beim Treiber
  std::vector<uint32_t> ready;    // fertige Bilder (Indizes)
  short poll_revents = POLLIN;
  int poll_errno = 0;
  int last_errno = 0;
  std::vector<std::vector<uint8_t>> memory;
};

class FakeSys : public vg::Sys {
 public:
  explicit FakeSys(FakeState* s) : s_(s) {}
  int Open(const char*, int) override {
    if (s_->open_errno != 0) {
      s_->last_errno = s_->open_errno;
      return -1;
    }
    return 7;
  }
  int Close(int) override {
    s_->closed = true;
    return 0;
  }
  int Ioctl(int, unsigned long req, void* arg) override {
    if (req == s_->fail_ioctl) {
      s_->last_errno = s_->fail_errno;
      return -1;
    }
    switch (req) {
      case VIDIOC_S_INPUT:
        s_->input = *static_cast<int*>(arg);
        return 0;
      case VIDIOC_S_STD:
        s_->std = *static_cast<v4l2_std_id*>(arg);
        return 0;
      case VIDIOC_S_FMT: {
        auto* f = static_cast<v4l2_format*>(arg);
        s_->width = f->fmt.pix.width;
        s_->height = f->fmt.pix.height;
        f->fmt.pix.bytesperline = f->fmt.pix.width * 2;
        return 0;
      }
      case VIDIOC_REQBUFS: {
        auto* r = static_cast<v4l2_requestbuffers*>(arg);
        s_->memory.assign(r->count,
                          std::vector<uint8_t>(s_->width * s_->height * 2));
        return 0;
      }
      case VIDIOC_QUERYBUF: {
        auto* b = static_cast<v4l2_buffer*>(arg);
        b->length = static_cast<uint32_t>(s_->memory[b->index].size());
        b->m.offset = b->index * 4096;
        return 0;
      }
      case VIDIOC_QBUF:
        s_->queued.push_back(static_cast<v4l2_buffer*>(arg)->index);
        return 0;
      case VIDIOC_DQBUF: {
        if (s_->ready.empty()) {
          s_->last_errno = EAGAIN;
          return -1;
        }
        auto* b = static_cast<v4l2_buffer*>(arg);
        b->index = s_->ready.front();
        s_->ready.erase(s_->ready.begin());
        b->bytesused = s_->width * s_->height * 2;
        b->timestamp.tv_sec = 2;
        b->timestamp.tv_usec = 5;
        return 0;
      }
      case VIDIOC_STREAMON:
        s_->streaming = true;
        return 0;
      case VIDIOC_STREAMOFF:
        s_->streaming = false;
        return 0;
    }
    return 0;
  }
  void* Mmap(size_t, int, long offset) override {
    return s_->memory[static_cast<size_t>(offset / 4096)].data();
  }
  int Munmap(void*, size_t) override {
    ++s_->munmaps;
    return 0;
  }
  int PollIn(int, int, short* revents) override {
    if (s_->poll_errno != 0) {
      s_->last_errno = s_->poll_errno;
      return -1;
    }
    if (s_->ready.empty() && (s_->poll_revents & POLLIN) != 0) {
      *revents = 0;
      return 0;  // Frist abgelaufen
    }
    *revents = s_->poll_revents;
    return 1;
  }
  int Errno() const override { return s_->last_errno; }

 private:
  FakeState* s_;
};

vg::V4l2Capture Make(FakeState* s) {
  return vg::V4l2Capture(std::make_unique<FakeSys>(s));
}

}  // namespace

TEST_CASE(oeffnen_stellt_pal_eingang_und_uyvy_ein) {
  FakeState s;
  auto cap = Make(&s);
  vg::CaptureConfig cfg;
  cfg.input = 0;
  EXPECT(cap.Open(cfg) == vg::OpenResult::kOk);
  EXPECT(s.input == 0);
  EXPECT(s.std == V4L2_STD_PAL);
  EXPECT(cap.width() == 720);
  EXPECT(cap.height() == 576);
  EXPECT(s.streaming);
  EXPECT(s.queued.size() == 4);
}

TEST_CASE(ntsc_hat_480_zeilen) {
  FakeState s;
  auto cap = Make(&s);
  vg::CaptureConfig cfg;
  cfg.pal = false;
  EXPECT(cap.Open(cfg) == vg::OpenResult::kOk);
  EXPECT(s.std == V4L2_STD_NTSC);
  EXPECT(cap.height() == 480);
}

TEST_CASE(fehlendes_geraet_heisst_missing) {
  FakeState s;
  s.open_errno = ENOENT;
  auto cap = Make(&s);
  EXPECT(cap.Open({}) == vg::OpenResult::kMissing);
  EXPECT(!cap.is_open());
}

TEST_CASE(fehler_beim_einstellen_raeumt_auf) {
  FakeState s;
  s.fail_ioctl = VIDIOC_STREAMON;
  auto cap = Make(&s);
  EXPECT(cap.Open({}) == vg::OpenResult::kError);
  EXPECT(!cap.is_open());
  EXPECT(s.closed);
  EXPECT(s.munmaps == 4);
  EXPECT(cap.last_error().find("VIDIOC_STREAMON") == 0);
}

TEST_CASE(ohne_quelle_kommt_nur_timeout) {
  FakeState s;
  auto cap = Make(&s);
  EXPECT(cap.Open({}) == vg::OpenResult::kOk);
  vg::Frame f;
  EXPECT(cap.Wait(500, &f) == vg::WaitResult::kTimeout);
  EXPECT(f.data == nullptr);
}

TEST_CASE(bild_wird_geliefert_und_zurueckgegeben) {
  FakeState s;
  auto cap = Make(&s);
  EXPECT(cap.Open({}) == vg::OpenResult::kOk);
  s.queued.clear();
  s.ready = {2};
  vg::Frame f;
  EXPECT(cap.Wait(500, &f) == vg::WaitResult::kFrame);
  EXPECT(f.index == 2);
  EXPECT(f.data == s.memory[2].data());
  EXPECT(f.width == 720 && f.height == 576 && f.stride == 1440);
  EXPECT(f.bytes == 720u * 576u * 2u);
  EXPECT(f.timestamp_us == 2000005u);
  cap.Release(f);
  EXPECT(s.queued.size() == 1 && s.queued[0] == 2);
}

TEST_CASE(abgezogen_wird_erkannt) {
  FakeState s;
  auto cap = Make(&s);
  EXPECT(cap.Open({}) == vg::OpenResult::kOk);
  s.ready = {0};
  s.poll_revents = POLLERR;
  vg::Frame f;
  EXPECT(cap.Wait(500, &f) == vg::WaitResult::kGone);

  s.poll_revents = POLLIN;
  s.poll_errno = ENODEV;
  EXPECT(cap.Wait(500, &f) == vg::WaitResult::kGone);
}

TEST_CASE(dqbuf_mit_enodev_ist_gone) {
  FakeState s;
  auto cap = Make(&s);
  EXPECT(cap.Open({}) == vg::OpenResult::kOk);
  s.ready = {1};
  s.fail_ioctl = VIDIOC_DQBUF;
  s.fail_errno = ENODEV;
  vg::Frame f;
  EXPECT(cap.Wait(500, &f) == vg::WaitResult::kGone);
}

TEST_CASE(schliessen_stoppt_strom_und_gibt_speicher_frei) {
  FakeState s;
  auto cap = Make(&s);
  EXPECT(cap.Open({}) == vg::OpenResult::kOk);
  cap.Close();
  EXPECT(!s.streaming);
  EXPECT(s.munmaps == 4);
  EXPECT(s.closed);
  vg::Frame f;
  EXPECT(cap.Wait(10, &f) == vg::WaitResult::kGone);
}

TEST_CASE(unvollstaendiges_bild_wird_erkannt) {
  vg::Frame f;
  f.stride = 1440;
  f.height = 480;
  f.bytes = 1440u * 480u;
  EXPECT(f.complete());
  f.bytes = 689156;  // gemessen auf jeep-pi mit Paketverlust
  EXPECT(!f.complete());
}

int main() { return check::RunAll(); }
