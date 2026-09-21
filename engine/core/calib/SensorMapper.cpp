#include "core/calib/SensorMapper.h"

namespace hexmap {

bool SensorMapper::sensorToOutput(const Vec2& sensor, Vec2& outputPx) const {
    if (m_calib == nullptr || !m_calib->isValid()) {
        m_lastFailure = MapFailure::NotCalibrated;
        return false;
    }

    outputPx = m_calib->sensorToOutput().transformPoint(sensor);
    if (!outputPx.isFinite()) {
        m_lastFailure = MapFailure::NotCalibrated;
        return false;
    }

    m_lastFailure = MapFailure::None;
    return true;
}

MappedPoint SensorMapper::map(const Vec2& sensor) const {
    MappedPoint out;

    if (!sensorToOutput(sensor, out.outputPx)) return out;

    if (m_screen == nullptr) {
        m_lastFailure = MapFailure::NoScreen;
        return out;
    }

    // Screen tự lo thứ tự z, solo/enabled, và lọc hộp bao.
    if (!m_screen->outputToCanvas(out.outputPx, out.sliceIndex,
                                  out.contentUV, out.canvasPx)) {
        m_lastFailure = MapFailure::NoSliceHit;
        return out;
    }

    m_lastFailure = MapFailure::None;
    out.valid = true;
    return out;
}

bool SensorMapper::contentToSensor(int sliceIndex,
                                   const Vec2& contentUV,
                                   Vec2& outSensor) const {
    if (m_calib == nullptr || !m_calib->isValid()) {
        m_lastFailure = MapFailure::NotCalibrated;
        return false;
    }
    if (m_screen == nullptr) {
        m_lastFailure = MapFailure::NoScreen;
        return false;
    }
    if (sliceIndex < 0 || sliceIndex >= m_screen->sliceCount()) {
        m_lastFailure = MapFailure::NoSliceHit;
        return false;
    }

    const Slice& s = m_screen->slices[static_cast<size_t>(sliceIndex)];
    const Vec2 outputPx = s.contentToOutput(contentUV);

    outSensor = m_calib->outputToSensor().transformPoint(outputPx);
    if (!outSensor.isFinite()) {
        m_lastFailure = MapFailure::NotCalibrated;
        return false;
    }

    m_lastFailure = MapFailure::None;
    return true;
}

} // namespace hexmap
