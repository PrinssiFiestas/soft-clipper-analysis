#version 430 core
#line 3

#define PI 3.14159265359
#define INT_MAX 2147483647
#define INT_MIN (-INT_MAX - 1)

#ifndef EXTERNAL_DEFS
#define BASE 64
#define T (7*BASE)
#define THD_NORMALIZED 0.0222559
#define SKIP 2
#define IIR_TAIL_LENGTH 8
#define IIR_INTENSITY 1
#define IIR_POLES 3
#define MAX_IN_GAIN 1.5
#define CACHE_LINE_SIZE 64
#define GPU_WORK_SIZE (1 << 3)
#define WORK_GROUP_SIZE 16
#endif // EXTERNAL_DEFS

// Index offset to middle element.
#define F_MID (IIR_TAIL_LENGTH + BASE)

struct Work
{
    uint  index_lo;
    uint  index_hi; // 64-bit index
    uint  state;
    float hardness;
    int   gen[1 + BASE];

    // Alignment and debugging.
    float pad[(CACHE_LINE_SIZE - ((4 + 1+BASE)*4) % CACHE_LINE_SIZE) / 4];
};

layout(local_size_x = WORK_GROUP_SIZE, local_size_y = 1, local_size_z = 1) in;
layout(std430, binding = 0) buffer work_buffer
{
    Work f[];
} work;

uniform uint g_work_length;

#define RESULT work.f[gl_GlobalInvocationID.x]
uint  f_state;
int   f_gen[1 + BASE];
float f[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];

bool f_next()
{
    if (f_gen[1] >= BASE)
        return false;

    int d1, d2;
    if (f_gen[f_state] < BASE) { // flush from next
        ++f_state;
    }
    else do { // flush from left
        --f_state;
        d1 = f_gen[f_state-0] - f_gen[f_state-1];
        d2 = f_gen[f_state-1] - f_gen[f_state-1-int(f_state>1)];
    } while (d1 == d2);

    int inc = f_gen[f_state] + 1;
    for (uint j = f_state; j < 1 + BASE; ++j) // flush
        f_gen[j] = inc;
    return true;
}

void f_filter()
{
    float f_right[IIR_TAIL_LENGTH + BASE + 1 + BASE + IIR_TAIL_LENGTH];

    // Copy positive side.
    for (uint i = 0; i <= BASE; ++i)
        f[F_MID + i] = f_right[F_MID + i] = f_gen[i];

    // Find first and second derivative for better extrapolation.
    float d1 = 0.;
    float d2 = 0.;
    for (int i1 = BASE - 1; i1 >= 0; --i1) {
        if (f_gen[i1] != f_gen[BASE]) {
            d1 = float(f_gen[BASE] - f_gen[i1]) / float(BASE - i1);
            for (int i2 = i1 - 1; i2 >= 0; --i2) {
                if (f_gen[i2] != f_gen[i1]) {
                    float _d2 = float(f_gen[i1] - f_gen[i2]) / float(i1 - i2);
                    d2 = (d1 -_d2) / float(i1 - i2);
                    break;
                }
            }
            break;
        }
    }
    float extra = f[F_MID + BASE];
    for (uint i = BASE + 1; i < 1 + BASE + IIR_TAIL_LENGTH; ++i) { // extrapolate
        extra += d1;
        d1 += d2;
        if (d1 < 0.f)
            d1 = d2 = 0;
        f[F_MID + i] = f_right[F_MID + i] = extra;
    }

    // Mirror negative side.
    for (uint i = 1; i < 1 + BASE + IIR_TAIL_LENGTH; ++i)
        f[F_MID + -i] = f_right[F_MID + -i] = -f[F_MID + i];

    for (uint k = 0; k < IIR_POLES; ++k) {
        // IIR filtering from right and left
        const float a = 1. / (1<<IIR_INTENSITY);
        const float b = 1. - a;
        for (int i = 1 + BASE + IIR_TAIL_LENGTH - 2; i >= -BASE - IIR_TAIL_LENGTH; --i)
            f_right[F_MID + i] = a*f_right[F_MID + i] + b*f_right[F_MID + i + 1];
        for (int i = -1 - BASE - IIR_TAIL_LENGTH + 2; i <= BASE + IIR_TAIL_LENGTH; ++i)
            f[F_MID + i] = a*f[F_MID + i] + b*f[F_MID + i - 1];
    }
    // Combine for zero phase. Scale as well for sensible ranges.
    for (int i = -BASE - IIR_TAIL_LENGTH; i <= BASE + IIR_TAIL_LENGTH; ++i)
        f[F_MID + i] = (1./BASE) * (f[F_MID + i] + f_right[F_MID + i]);
}

// First and second derivative. First will be in [0], second in [1].
vec2 f_tail_derivatives()
{
    float d1 = f[F_MID + BASE] - f[F_MID + BASE - 1];
    float d2 = d1 - (f[F_MID + BASE - 1] - f[F_MID + BASE - 2]);
    return vec2(d1, d2);
}

float f_call(float x, vec2 f_ds)
{
    float t = float(BASE)*x;
    int i = int(floor(t)) >= INT_MAX ? INT_MAX : int(floor(t)) < INT_MIN ? INT_MIN : int(floor(t));

    if (-BASE <= i && i < BASE) // interpolate
        return (1.-fract(t))*f[F_MID + i] + fract(t)*f[F_MID + i+1];
    // else extrapolate.

    float d1 = f_ds[0]; // first derivative
    float d2 = f_ds[1]; // second derivative

    if (d1 <= 0.)
        return i >= BASE ? f[F_MID + BASE] : f[F_MID + -BASE];

    // Quadratic polynomial P(x) = a*x^2 + b*x + c. Coefficients a and b can be
    // solved from derivatives P''(BASE) = 2*a = d2
    // and P'(BASE) = 2*a*BASE + b = d1. c can be solved from P(BASE) = f(BASE).
    float a = .5*d2;
    float b = d1 - 2.*a*(float(BASE)-1.);
    float c = f[F_MID + BASE] - a*float(BASE*BASE) - b*float(BASE);

    if (a == 0.) // avoid zero division
        return b*t + c;

    // Clamp at the top of P(x), which can be found using P'(x_max) = 0.
    float x_max = -b/(2.*a);
    if (abs(t) >= x_max) {
        float y = a*x_max*x_max + b*x_max + c;
        return t >= 0. ? y : -y;
    }
    float y = a*abs(t)*abs(t) + b*abs(t) + c;
    return t >= 0. ? y : -y;
}

float f_call(float x)
{
    return f_call(x, f_tail_derivatives());
}

vec4 f_call(vec4 x, vec2 f_ds)
{
    vec4 t = float(BASE)*x;
    ivec4 it = ivec4(floor(t));
    ivec4 i = ivec4(clamp(it, ivec4(-BASE), ivec4(BASE - 1)));

    vec4 interpolated = vec4(
        mix(f[F_MID + i.x], f[F_MID + i.x + 1], fract(t.x)),
        mix(f[F_MID + i.y], f[F_MID + i.y + 1], fract(t.y)),
        mix(f[F_MID + i.z], f[F_MID + i.z + 1], fract(t.z)),
        mix(f[F_MID + i.w], f[F_MID + i.w + 1], fract(t.w)));

    float d1 = f_ds[0]; // first derivative
    float d2 = f_ds[1]; // second derivative

    if (d1 <= 0.)
        return mix(interpolated, sign(x)*f[F_MID+BASE], step(vec4(1), abs(x)));

    float a = .5*d2;
    float b = d1 - 2.*a*(float(BASE)-1.);
    float c = f[F_MID + BASE] - a*float(BASE*BASE) - b*float(BASE);

    if (a == 0.) // avoid zero division
        return mix(interpolated, b*t + c, step(vec4(1), abs(x)));

    vec4 x_max = min(abs(t), vec4(-b/(2.*a)));
    vec4 x_extra = min(abs(t), x_max);
    vec4 extrapolated = sign(t) * (a*x_extra*x_extra + b*x_extra + c);
    return mix(interpolated, extrapolated, step(vec4(1), abs(x)));
}

vec4 f_call(vec4 x)
{
    return f_call(x, f_tail_derivatives());
}

bool is_equal_float(float a, float b, float max_relative_diff)
{
    a = abs(a);
    b = abs(b);
    return abs(a - b) < max_relative_diff * max(a, b);
}

float fmaf(float x, float y, float z)
{
    return x * y + z;
}

// https://stackoverflow.com/questions/27229371/inverse-error-function-in-c
// compute inverse error functions with maximum error of 2.35793 ulp
float erfinvf(float a)
{
    float p, r, t;
    t = fmaf (a, 0.0 - a, 1.0);
    t = log (t);
    if (abs(t) > 6.125) { // maximum ulp error = 2.35793
        p =              3.03697567e-10; //  0x1.4deb44p-32
        p = fmaf (p, t,  2.93243101e-8); //  0x1.f7c9aep-26
        p = fmaf (p, t,  1.22150334e-6); //  0x1.47e512p-20
        p = fmaf (p, t,  2.84108955e-5); //  0x1.dca7dep-16
        p = fmaf (p, t,  3.93552968e-4); //  0x1.9cab92p-12
        p = fmaf (p, t,  3.02698812e-3); //  0x1.8cc0dep-9
        p = fmaf (p, t,  4.83185798e-3); //  0x1.3ca920p-8
        p = fmaf (p, t, -2.64646143e-1); // -0x1.0eff66p-2
        p = fmaf (p, t,  8.40016484e-1); //  0x1.ae16a4p-1
    } else { // maximum ulp error = 2.35002
        p =              5.43877832e-9;  //  0x1.75c000p-28
        p = fmaf (p, t,  1.43285448e-7); //  0x1.33b402p-23
        p = fmaf (p, t,  1.22774793e-6); //  0x1.499232p-20
        p = fmaf (p, t,  1.12963626e-7); //  0x1.e52cd2p-24
        p = fmaf (p, t, -5.61530760e-5); // -0x1.d70bd0p-15
        p = fmaf (p, t, -1.47697632e-4); // -0x1.35be90p-13
        p = fmaf (p, t,  2.31468678e-3); //  0x1.2f6400p-9
        p = fmaf (p, t,  1.15392581e-2); //  0x1.7a1e50p-7
        p = fmaf (p, t, -2.32015476e-1); // -0x1.db2aeep-3
        p = fmaf (p, t,  8.86226892e-1); //  0x1.c5bf88p-1
    }
    r = a * p;
    return r;
}

float probitf(float p)
{
    return sqrt(2.) * erfinvf(2.*p - 1.);
}

float f_thd(float in_gain)
{
    vec2 ds = f_tail_derivatives();
    vec4 b0 = vec4(0);
    const float dt = 1./float(T);
    vec4 t = vec4(0, dt, 2.*dt, 3.*dt);
    for (; t.x < 1. - 4.*dt; t += 4.*dt)
        b0 += f_call(in_gain*sin(2.*PI*t), ds) * sin(2.*PI*t);
    b0 += step(t, vec4(1)) * f_call(in_gain*sin(2.*PI*t), ds) * sin(2.*PI*t);
    float B0 = b0.x + b0.y + b0.z + b0.w;

    float sum = 0.;
    for (vec4 k = vec4(3, 5, 7, 9); k.w < float(T/2 - SKIP/2); k += 8.) {
        vec4 b = vec4(0);
        for (float t = 0.; t < 1.; t += dt)
            b += f_call(in_gain*sin(2.*PI*t), ds) * sin(2.*PI*k*t);
        sum += dot(b, b);

        if (abs(b.w/B0) < .001)
            break;
    }
    return sum / (B0*B0);
}

float normalized_input_gain()
{
    // Plotting many clipper's THD's as functions of input gains showed that
    // most clippers have close to zero THD when x < 0.3. Same plots revealed
    // that the average of x == .6 got same THD as the Blunter's THD. So we
    // start our estimate with that average and use the minimum to get next data
    // point.
    const float x_min = .3;
    const float y_min = -THD_NORMALIZED; // can't have THD < 0
    const float x_avg = .6;

    float x0 = x_avg;
    float y0 = f_thd(x0) - THD_NORMALIZED;

    // Line between average minimum point (x_min, y_min) to first data point
    // (x0, y0) is described by k*(x-x_min) + y0.
    float k = (y0 - y_min) / (x0 - x_min);

    // Solving for next x1 from k*(x1-x_min) + y0 == 0 gives us
    float x1 = x_min - y_min/k;
    float y1 = f_thd(x1) - THD_NORMALIZED;

    // We have two data points now, use secant method to find final result fast.
    // First secant iterations might throw x to negative root. This is fine
    // because f_thd(-kx) == f_thd(kx), but the results will be confusing
    // down the line, so return absolute value.
    float x2 = x1;
    float y2 = y1;
    uint secant_iterations = 0;
    while (abs(y2) > .01 * THD_NORMALIZED) {
        secant_iterations++; // Note: less max iters than CPU to avoid GPU hang.
        if (secant_iterations > 6 || y1 == y0)
                return 1e10;

        x2 = x1 - y1 * (x1 - x0) / (y1 - y0);
        y2 = f_thd(x2) - THD_NORMALIZED;
        x0 = x1;
        x1 = x2;
        y0 = y1;
        y1 = y2;
    }
    return abs(x2);
}

float normalized_output_gain(float input_gain)
{
    float sum = 0.; // of squares
    const float dt = .5/float(BASE);
    for (float t = dt; t < 1.; t += dt) {
        float x = f_call(input_gain*probitf(t));
        sum += x*x;
    }
    return 1. / sqrt(dt*sum);
}

float f_hardness(float out_gain, float in_gain)
{
    // Check if too much data out of bounds for reliable results. Experiment
    // showed that most input gains are well below 1 anyway.
    if (in_gain > MAX_IN_GAIN) // don't try to extrapolate.
        return 1e20; // discard

    // If f_normalized(x) = out_gain*f(in_gain*x), then chain rule gives us
    // f_normalized''(x) = out_gain*in_gain^2*f''(in_gain*x). We don't need to
    // care about x, we just need the minimum value of the second derivative.
    float d_min = 0.;
    for (uint i = 0; i <= BASE; ++i) {
        float d0 = f[F_MID + i-0] - f[F_MID + i-1];
        float d1 = f[F_MID + i+1] - f[F_MID + i+0];
        d_min = min(d_min, d1 - d0);
    }
    float hardness = -out_gain * in_gain * in_gain * d_min * BASE * BASE;
    if (hardness <= .1) // impossible, bug somewhere
        return 1e20;
    if (in_gain <= 1.) // all data included, can trust result.
        return hardness;
    // else check if we have to extrapolate.

    // We need some estimate if the min would fall. The problem is that our
    // second derivative is already very noisy, so we'll average out some range
    // at the end to estimate the trend.
    float third_derivative_sum  = 0.;
    for (uint i = 2*BASE/3; i <= BASE; ++i) {
        float d0 = f[F_MID + i-0] - f[F_MID + i-1];
        float d1 = f[F_MID + i+1] - f[F_MID + i+0];
        float d2 = f[F_MID + i+2] - f[F_MID + i+1];
        third_derivative_sum += (d2 - d1) - (d1 - d0);
    }
    if (third_derivative_sum >= 0.) // min not likely to change, can trust result.
        return hardness;
    // else safer to just discard.
    return 1e20;
}

void main()
{
    if (gl_GlobalInvocationID.x >= g_work_length)
        return;

    uint f_index_hi = RESULT.index_hi;
    uint f_index    = RESULT.index_lo;
    f_gen           = RESULT.gen;
    f_state         = RESULT.state;
    RESULT.hardness = 1e10;

    do {
        f_filter();
        float in_gain = normalized_input_gain();
        if (in_gain > MAX_IN_GAIN)
            continue;
        float out_gain = normalized_output_gain(in_gain);
        float hardness = f_hardness(out_gain, in_gain);
        if (hardness < RESULT.hardness) {
            RESULT.index_hi = f_index_hi;
            RESULT.index_lo = f_index;
            RESULT.state    = f_state;
            RESULT.hardness = hardness;
            RESULT.gen      = f_gen;
            #if GPU_DEBUG
            RESULT.pad[0]   = in_gain;
            RESULT.pad[1]   = out_gain;
            #endif
        }
        ++f_index;
        if (f_index == 0)
            ++f_index_hi;
    } while ((f_index & uint(GPU_WORK_SIZE-1)) > 0 && f_next());
}
