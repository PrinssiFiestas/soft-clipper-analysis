#include "shared.h"
#include <float.h>

// Source:
// https://www.jb101.co.uk/2020/12/27/monotone-cubic-interpolation.html

// Monotone cubic interpolation of values y0 and y1 using x as the interpolation
// parameter (assumed to be [0..1]). A modification of hermite cubic interpolation
// that prevents overshoots (preserves monoticity). In order to both maintain
// monotonicity and C1 continuity, two neighbouring samples to the left of y0 and
// the right of y1 are also necessary
float interpolate_cubic_monotonic_heckbert(
    float x,
    float y_minus_1,
    float y0,
    float y1,
    float y2)
{
    // Calculate secant line gradients for each successive pair of data points
    const float s_minus_1 = y0 - y_minus_1;
    const float s_0 = y1 - y0;
    const float s_1 = y2 - y1;

    // Use central differences to calculate initial gradients at the end-points
    float m_0 = (s_minus_1 + s_0) * 0.5f;
    float m_1 = (s_0 + s_1) * 0.5f;

    // If the central curve (joining y0 and y1) is neither increasing or decreasing, we
    // should have a horizontal line, so immediately set gradients to zero here.
    if (is_equal_float(y0, y1, FLT_EPSILON)) {
        m_0 = 0.0f;
        m_1 = 0.0f;
    }
    else {
        // If the curve to the left is horizontal, or the sign of the secants on either side
        // of the end-point are different, set the gradient to zero...
        if (is_equal_float(y_minus_1, y0, FLT_EPSILON) || (s_minus_1 < 0.0f && s_0 >= 0.0f) || (s_minus_1 > 0.0f && s_0 <= 0.0f)) {
            m_0 = 0.0f;
        }
        // ... otherwise, ensure the magnitude of the gradient is constrained to 3 times the
        // left secant, and 3 times the right secant (whatever is smaller)
        else {
            m_0 = fminf(fminf(3.0f * s_minus_1 / m_0, 3.0f * s_0 / m_0), 1.0f);
        }

        // If the curve to the right is horizontal, or the sign of the secants on either side
        // of the end-point are different, set the gradient to zero...
        if (is_equal_float(y1, y2, FLT_EPSILON) || (s_0 < 0.0f && s_1 >= 0.0f) || (s_0 > 0.0f && s_1 <= 0.0f)) {
            m_1 = 0.0f;
        }
        // ... otherwise, ensure the magnitude of the gradient is constrained to 3 times the
        // left secant, and 3 times the right secant (whatever is smaller)
        else {
            m_1 *= fminf(fminf(3.0f * s_0 / m_1, 3.0f * s_1 / m_1), 1.0f);
        }
    }

    // Evaluate the cubic Hermite spline
    float result =
        (((((m_0 + m_1 - 2.0f * s_0) * x) + (3.0f * s_0 - 2.0f * m_0 - m_1)) * x) + m_0) * x + y0;

    // The values at the end points (y0 and y1) define an interval that the curve passes
    // through. Since the curve between the end-points is now monotonic, all interpolated
    // values between these end points should be inside this interval. However, floating
    // point rounding error can still lead to values slightly outside this range.
    // Guard against this by clamping the interpolated result to this interval...
    float min = fminf(y0, y1);
    float max = fmaxf(y0, y1);
    return fmax(fmin(result, max), min);
}
