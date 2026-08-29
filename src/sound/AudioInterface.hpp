#ifndef AUDIOINTERFACE_HPP
#define AUDIOINTERFACE_HPP

#include <array>
#include <atomic>
#include <vector>

#define SAMPLING_FREQ (44100)

namespace sound {

class AudioInterface {
 public:
  static constexpr std::size_t SamplesTableSize = 512;
  using Sample = float;
  using MonoSamples = std::array<Sample, SamplesTableSize>;

  struct StereoSample {
    Sample left, right;
  };

  virtual ~AudioInterface() = default;
  virtual bool queue_stereo_samples(const MonoSamples&, const MonoSamples&) = 0;
  virtual float mix(const std::vector<float>&) const = 0;
  virtual void start() = 0;
  virtual void terminate() = 0;
  virtual void toggle_mute() = 0;

  virtual operator bool() const = 0;

  // Playback normally paces the emulator: the APU spins until the sound card
  // drains a buffer. Headless drivers set this to run as fast as they can.
  void set_free_running(bool v) { _free_running = v; }
  bool free_running() const { return _free_running; }

 private:
  std::atomic_bool _free_running{false};
};

}  // namespace sound

#endif  // AUDIOINTERFACE_HPP
