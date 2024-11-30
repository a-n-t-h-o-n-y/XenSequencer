#include <memory>
#include <string>

namespace xen
{

/**
 * Abstract base class for modulators.
 */
class Modulator
{
  public:
    virtual ~Modulator() = default;

  public:
    /**
     * @brief Get the value of the modulator at time t.
     * @param t The time to get the value at. A full cycle is over the range [0, 1].
     * @return The value of the modulator at time t. No range is guaranteed.
     */
    [[nodiscard]] virtual auto operator()(float t) -> float = 0;
};

/**
 * A modulator that returns a constant value.
 */
class ConstantModulator : public Modulator
{
  public:
    /**
     * Construct a new Constant Modulator object.
     * @param value The constant value to return.
     */
    explicit ConstantModulator(float value);

  public:
    [[nodiscard]] auto operator()(float) -> float override;

  private:
    float value_;
};

/**
 * A modulator that returns a sine wave.
 */
class SineModulator : public Modulator
{
  public:
    SineModulator(float amplitude, float frequency, float phase);

  public:
    [[nodiscard]] auto operator()(float t) -> float override;

  private:
    float amplitude_;
    float frequency_;
    float phase_;
};

/**
 * A modulator that returns a triangle wave.
 */
class TriangleModulator : public Modulator
{
  public:
    TriangleModulator(float amplitude, float frequency, float phase);

  public:
    [[nodiscard]] auto operator()(float t) -> float override;

  private:
    float amplitude_;
    float frequency_;
    float phase_;
};

/**
 * A modulator that returns a sawtooth wave starting at 0.
 */
class SawtoothUpModulator : public Modulator
{
  public:
    SawtoothUpModulator(float amplitude, float frequency, float phase);

  public:
    [[nodiscard]] auto operator()(float t) -> float override;

  private:
    float amplitude_;
    float frequency_;
    float phase_;
};

/**
 * A modulator that returns a sawtooth wave starting at 1.
 */
class SawtoothDownModulator : public Modulator
{
  public:
    SawtoothDownModulator(float amplitude, float frequency, float phase);

  public:
    [[nodiscard]] auto operator()(float t) -> float override;

  private:
    float amplitude_;
    float frequency_;
    float phase_;
};

/**
 * A modulator that returns a square wave.
 */
class SquareModulator : public Modulator
{
  public:
    /**
     * @throws std::invalid_argument if pulse_width is not in the range [0, 1].
     */
    SquareModulator(float amplitude, float frequency, float phase, float pulse_width);

  public:
    [[nodiscard]] auto operator()(float t) -> float override;

  private:
    float amplitude_;
    float frequency_;
    float phase_;
    float pulse_width_;
};

/**
 * A modulator that returns a random noise wave.
 */
class NoiseModulator : public Modulator
{
  public:
    explicit NoiseModulator(float amplitude);

  public:
    [[nodiscard]] auto operator()(float t) -> float override;

  private:
    float amplitude_;
};

} // namespace xen