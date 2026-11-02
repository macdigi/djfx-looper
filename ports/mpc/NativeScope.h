#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>

// Original, self-contained display analysis for the source edition. MPC draws
// Legacy display parameters remain reserved. The current native renderer reads
// coherent frames below; no private QuotaScope protocol or audio allocation.
class DJFXNativeScope {
public:
    static constexpr int frameSamples = 2048;
    struct Frame {
        std::array<float, frameSamples> left{}, right{};
        unsigned sequence = 0;
        float sampleRate = 44100;
    };
    static_assert(std::atomic<float>::is_always_lock_free, "Display publication must be lock-free");
    static constexpr int columns = 64;
    static constexpr int fftSize = 512;
    static constexpr int levels = 32;

    DJFXNativeScope() { setRate(44100); clear(); }
    void setRate(double value) noexcept {
        rate = std::clamp(value, 8000.0, 192000.0);
        interval = std::max(1, int(std::lround(rate / 30.0)));
        for (int i = 0; i < fftSize; ++i)
            window[i] = .5f - .5f * std::cos(float(2 * pi * i / (fftSize - 1)));
        for (int i = 0; i < fftSize / 2; ++i)
            twiddle[i] = {float(std::cos(-2 * pi * i / fftSize)), float(std::sin(-2 * pi * i / fftSize))};
        clear();
    }
    void clear() noexcept {
        left.fill(0); right.fill(0); waterfall.fill(0); write = elapsed = 0;
        rawLeft.fill(0); rawRight.fill(0); rawWrite=0;
        for (auto& value : display) value.store(0, std::memory_order_relaxed);
        for (auto& value : banks) value.store(0, std::memory_order_relaxed);
    }
    void setMode(int value) noexcept { mode = std::clamp(value, 0, 6); }
    void setEnabled(bool value) noexcept { enabled = value; }
    void setVisuals(bool, float, int color, int gainDb) noexcept {
        palette = std::clamp(color, 0, 2);
        gain = std::pow(10.f, std::clamp(gainDb, 0, 18) / 20.f);
    }
    int displayIdentity() const noexcept { return 64; } // Retained legacy slot.
    int modeValue() const noexcept { return mode.load(std::memory_order_relaxed); }
    int paletteValue() const noexcept { return palette.load(std::memory_order_relaxed); }
    bool enabledValue() const noexcept { return enabled.load(std::memory_order_relaxed); }
    void setPalette(int value) noexcept { palette=std::clamp(value,0,2); }
    // Audio never waits for graphics. Every published sample is atomic; a
    // bounded sequence check gives readers a coherent frame without a data race.
    bool readFrame(Frame& result) const noexcept {
        Frame candidate;
        for(int attempt=0;attempt<2;++attempt) {
            const unsigned before=frameSequence.load(std::memory_order_acquire);
            if(!before||(before&1))continue;
            for(int i=0;i<frameSamples;++i) {
                candidate.left[i]=publishedLeft[i].load(std::memory_order_relaxed);
                candidate.right[i]=publishedRight[i].load(std::memory_order_relaxed);
            }
            candidate.sampleRate=publishedRate.load(std::memory_order_relaxed);
            const unsigned after=frameSequence.load(std::memory_order_acquire);
            if(before==after){candidate.sequence=after;result=candidate;return true;}
        }
        return false;
    }
    float column(int index) const noexcept {
        return index >= 0 && index < columns ? display[index].load(std::memory_order_relaxed) : 0.f;
    }
    float bank(int index) const noexcept {
        return index >= 0 && index < columns ? banks[index].load(std::memory_order_relaxed) : 0.f;
    }
    void push(float** audio, int frames) noexcept {
        if (!audio || !audio[0] || frames <= 0) return;
        for (int i = 0; i < frames; ++i) {
            left[write] = finite(audio[0][i]);
            right[write] = finite(audio[1] ? audio[1][i] : audio[0][i]);
            rawLeft[rawWrite]=left[write];rawRight[rawWrite]=right[write];rawWrite=(rawWrite+1)&(frameSamples-1);
            write = (write + 1) & (fftSize - 1);
            if (++elapsed >= interval) { elapsed = 0; draw(); }
        }
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    double rate = 44100;
    int interval = 1470, elapsed = 0, write = 0;
    std::atomic<int> mode{0}, palette{0};
    float gain = 2;
    std::atomic<bool> enabled{true};
    std::array<float, frameSamples> rawLeft{},rawRight{};
    std::array<std::atomic<float>, frameSamples> publishedLeft{},publishedRight{};
    std::atomic<unsigned> frameSequence{0};std::atomic<float> publishedRate{44100};int rawWrite=0;
    std::array<float, fftSize> left{}, right{}, window{};
    std::array<std::complex<float>, fftSize> transform{};
    std::array<std::complex<float>, fftSize / 2> twiddle{};
    std::array<float, columns> waterfall{};
    std::array<std::atomic<float>, columns> display{};
    std::array<std::atomic<float>, columns> banks{};
    static float finite(float v) noexcept { return std::isfinite(v) ? std::clamp(v, -8.f, 8.f) : 0; }
    float sample(const std::array<float, fftSize>& channel, int offset) const noexcept {
        return channel[(write + offset) & (fftSize - 1)];
    }
    void scalar(int i, float value) noexcept {
        const int level = int(std::lround(std::clamp(value, 0.f, 1.f) * (levels - 1)));
        display[i].store(float(1 + palette * levels + level) / float(3 * levels), std::memory_order_relaxed);
        banks[i].store(0, std::memory_order_relaxed);
    }
    void point(int i, float x, float y) noexcept {
        const auto quantize = [](float v) { return int(std::lround((std::clamp(v, -1.f, 1.f) + 1) * 7.5f)); };
        const int cell = quantize(x) + 16 * quantize(y);
        display[i].store(float(1 + cell % 128) / 128.f, std::memory_order_relaxed);
        banks[i].store(cell >= 128 ? 1.f : 0.f, std::memory_order_relaxed);
    }
    void spectrum() noexcept {
        for (int i = 0; i < fftSize; ++i) {
            unsigned reversed = 0, bits = unsigned(i);
            for (int k = 0; k < 9; ++k) { reversed = (reversed << 1) | (bits & 1); bits >>= 1; }
            transform[reversed] = {.5f * (sample(left, i) + sample(right, i)) * window[i], 0};
        }
        for (int size = 2; size <= fftSize; size *= 2)
            for (int start = 0; start < fftSize; start += size)
                for (int j = 0; j < size / 2; ++j) {
                    const auto a = transform[start + j];
                    const auto b = transform[start + j + size / 2] * twiddle[j * fftSize / size];
                    transform[start + j] = a + b; transform[start + j + size / 2] = a - b;
                }
    }
    float band(int i, int bands) const noexcept {
        const float high = std::min(18000.f, float(rate * .49));
        const auto bin = [&](float fraction) {
            return std::clamp(int(std::lround(40.f * std::pow(high / 40.f, fraction) * fftSize / rate)), 1, fftSize / 2 - 1);
        };
        const int lo = bin(float(i) / bands), hi = bin(float(i + 1) / bands);
        float amplitude = 0;
        for (int b = lo; b <= hi; ++b) amplitude = std::max(amplitude, std::abs(transform[b]) * (4.f / fftSize));
        return std::clamp((20.f * std::log10(std::max(1.e-6f, amplitude)) + 60.f) / 60.f, 0.f, 1.f);
    }
    void draw() noexcept {
        frameSequence.fetch_add(1,std::memory_order_acq_rel);
        for(int i=0;i<frameSamples;++i){publishedLeft[i].store(rawLeft[(rawWrite+i)&(frameSamples-1)],std::memory_order_relaxed);publishedRight[i].store(rawRight[(rawWrite+i)&(frameSamples-1)],std::memory_order_relaxed);}
        publishedRate.store(float(rate),std::memory_order_relaxed);frameSequence.fetch_add(1,std::memory_order_release);
        if (!enabled) { for (auto& value : display) value.store(0, std::memory_order_relaxed); return; }
        if (mode == 2 || mode == 4) spectrum();
        if (mode == 4) {
            for (int i = columns - 1; i >= 16; --i) waterfall[i] = waterfall[i - 16];
            for (int i = 0; i < 16; ++i) waterfall[i] = band(i, 16);
        }
        for (int i = 0; i < columns; ++i) {
            if (mode == 2) { scalar(i, band(i, columns)); continue; }
            if (mode == 4) { scalar(i, waterfall[i]); continue; }
            const int offset = i * fftSize / columns;
            const float l = sample(left, offset), r = sample(right, offset);
            if (mode == 3) { point(i, (l + r) * .5f * gain, (l - r) * .5f * gain); continue; }
            if (mode == 5) { point(i, l * gain, sample(left, offset + 11) * gain); continue; }
            if (mode == 1) { scalar(i, .5f + .48f * std::clamp((l + r) * .5f * gain, -1.f, 1.f)); continue; }
            float value = 0;
            for (int j = 0; j < fftSize / columns; ++j) {
                const float a = sample(left, offset + j), b = sample(right, offset + j);
                if (mode == 6) value += .5f * (a * a + b * b);
                else value = std::max(value, std::max(std::abs(a), std::abs(b)));
            }
            scalar(i, gain * (mode == 6 ? std::sqrt(value / (fftSize / columns)) : value));
        }
    }
};
