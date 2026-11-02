#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace qfx
{
// Independently implemented from the recovered DJFX mathematical specification.
// No firmware, vendor executable, interpreter, or vendor code is shipped here.
// The three-sample I/O pipeline is represented explicitly: queued read taps and
// a one-frame delayed history write. prepare() is the only allocating method.
class DJFXKernel
{
public:
    static constexpr double normalization = 16384.0;
    static constexpr double zeroQuantum = 0x1p-32;
    using Stereo = std::array<double, 2>;
    struct Coefficients
    {
        double fadeIncrement, spanCap, fadeSamples;
        double audioB0, audioB1, audioA1;
        double lengthB0, lengthB1, lengthA1;
    };
    struct State
    {
        double phase = 0.0, fade = 0.0;
        double previousLength = 0.0, lengthHP = 0.0, smoothedLength = 0.0;
        Stereo previousInput {}, previousOutput {};
        Stereo movingTap {}, fixedTap {}, pendingWrite {};
    };

    void prepare (double sampleRate)
    {
        rate = std::isfinite (sampleRate) && sampleRate >= 8000.0
                 ? std::min (sampleRate, 768000.0) : 48000.0;
        spanSamples = static_cast<int> (std::ceil (10240.0 * rate / 44100.0));
        std::size_t size = 1;
        while (size < static_cast<std::size_t> (spanSamples + 2 * static_cast<int> (std::ceil (0.003 * rate)) + 16)) size <<= 1;
        for (auto& channel : history) channel.assign (size, 0.0);
        mask = size - 1;
        const auto hp = [this] (double hz)
        {
            const auto t = std::tan (3.14159265358979323846 * hz / rate);
            const auto a = (1.0 - t) / (1.0 + t);
            return std::array<double, 3> { (1.0 + a) * 0.5, -(1.0 + a) * 0.5, a };
        };
        const auto audio = hp (40.0), length = hp (2.0);
        coefficients = { 1.0 / (0.003 * rate), (spanSamples - 1) / normalization,
                         0.003 * rate, audio[0], audio[1], audio[2],
                         length[0], length[1], length[2] };
        // The measured 48 kHz firmware literals differ slightly from ideal
        // coefficients, including a fade increment just below exact 1/144.
        if (rate == 48000.0)
            coefficients = { 0.006944417953491211, (spanSamples - 1) / normalization,
                             144.0, 0.9973878860473633, -0.9973878860473633,
                             0.994776725769043, 0.9998683929443359,
                             -0.9998683929443359, 0.9997377395629883 };
        reset();
    }

    void reset() noexcept
    {
        for (auto& channel : history) std::fill (channel.begin(), channel.end(), 0.0);
        writeIndex = 0;
        state = {};
    }

    // QuotaFX's performance lifecycle preserves the live history while restarting
    // a gesture. Seed Length at the visible value; don't clear or copy audio.
    void resetPlayback() noexcept
    {
        state.phase = state.fade = 0.0;
        state.previousLength = state.smoothedLength = targetLength;
        state.lengthHP = 0.0;
        state.previousInput = state.previousOutput = {};
        state.movingTap = read (0.0);
        state.fixedTap = read (targetLength);
    }

    void setParameters (double seconds, double speed) noexcept
    {
        seconds = std::isfinite (seconds) ? std::clamp (seconds, 0.012, 0.230) : 0.230;
        playbackSpeed = std::isfinite (speed) ? std::clamp (speed, -1.0, 1.0) : 1.0;
        const auto samples = std::clamp (std::floor (seconds * rate - 2.5),
                                        0.0, static_cast<double> (spanSamples - 1));
        targetLength = samples / normalization;
    }

    Stereo process (double left, double right, double loopMix,
                    double speedGain = 1.0, double level = 1.0) noexcept
    {
        if (history[0].empty()) return { left, right };
        const Stereo input { finite (left), finite (right) };
        loopMix = std::clamp (finite (loopMix), 0.0, 1.0);
        speedGain = std::clamp (finite (speedGain), 0.0, 1.0);
        level = finite (level);
        Stereo feedback {}, output {};
        const auto nextMoving = read (state.phase);
        for (std::size_t c = 0; c < 2; ++c)
        {
            feedback[c] = std::fma (state.fixedTap[c] - input[c], loopMix, input[c]);
            const auto wet = state.movingTap[c] * state.fade;
            const auto filtered = std::fma (coefficients.audioA1, state.previousOutput[c],
                                  std::fma (coefficients.audioB1, state.previousInput[c],
                                            coefficients.audioB0 * wet));
            state.previousInput[c] = wet;
            state.previousOutput[c] = filtered;
            output[c] = filtered * (speedGain * level);
        }
        const auto lengthHP = std::fma (coefficients.lengthA1, state.lengthHP,
                              std::fma (coefficients.lengthB1, state.previousLength,
                                        coefficients.lengthB0 * targetLength));
        state.previousLength = targetLength;
        state.lengthHP = lengthHP;
        state.smoothedLength = targetLength - lengthHP;
        const auto step = std::fma (-playbackSpeed, 1.0 / normalization, 1.0 / normalization);
        const auto trial = state.phase + step;
        const auto cap = playbackSpeed == 1.0 ? 0.0 : coefficients.spanCap;
        const auto edge = std::min (state.smoothedLength, cap)
                          - std::fma (coefficients.fadeSamples, step, trial);
        state.fade = std::clamp (state.fade + (edge > -zeroQuantum
                                               ? coefficients.fadeIncrement
                                               : -coefficients.fadeIncrement), 0.0, 1.0);
        state.phase = state.fade < zeroQuantum ? 0.0 : trial;
        const auto nextFixed = read (state.smoothedLength);
        for (std::size_t c = 0; c < 2; ++c)
            history[c][writeIndex] = state.pendingWrite[c];
        writeIndex = (writeIndex + 1) & mask;
        state.pendingWrite = feedback;
        state.movingTap = nextMoving;
        state.fixedTap = nextFixed;
        return output;
    }

    const State& getState() const noexcept { return state; }
    const Coefficients& getCoefficients() const noexcept { return coefficients; }
    double getTargetLength() const noexcept { return targetLength; }
    std::size_t allocatedHistorySamples() const noexcept { return history[0].size(); }

private:
    static double finite (double x) noexcept { return std::isfinite (x) ? x : 0.0; }
    Stereo read (double normalizedDelay) const noexcept
    {
        // Valid control histories stay inside this bound. Clamp protects invalid
        // hosts/denormals without reading future or out-of-allocation samples.
        const auto distance = std::clamp (finite (normalizedDelay) * normalization + 1.0,
                                         1.0, static_cast<double> (mask - 1));
        const auto whole = static_cast<std::size_t> (distance);
        const auto fraction = distance - static_cast<double> (whole);
        const auto a = (writeIndex - whole) & mask;
        const auto b = (a - 1) & mask;
        Stereo result {};
        for (std::size_t c = 0; c < 2; ++c)
            result[c] = std::fma (history[c][b] - history[c][a], fraction, history[c][a]);
        return result;
    }
    std::array<std::vector<double>, 2> history;
    std::size_t writeIndex = 0, mask = 0;
    double rate = 48000.0, targetLength = 0.0, playbackSpeed = 1.0;
    int spanSamples = 11146;
    Coefficients coefficients {};
    State state {};
};
}
