__device__ void rgb2hslcuda(double r, double g, double b, double *h, double *s, double *l)
{
  // Copied from the CPU version, dunno if it's optimal for GPU
  // rgb are 0->1, return hsl as 0->100
  double min, max, delta;

  min = r < g ? r : g;
  min = min  < b ? min : b;

  max = r > g ? r : g;
  max = max  > b ? max  : b;

  // I seems these rgb values can be > 1. I'm going to just clamp them,
  // but maybe the right thing would be to assume a range of 0->highest_value_in_image,
  // rather than 1->1? Not sure.
  *l = max >= 1.0 ? 100.0 : 100.0 * max;

  delta = max - min;
  if (delta < 0.00001)
  {
      *s = 0;
      *h = 0;
      return;
  }
  if( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
      *s = 100 * (delta / max);
  } else {
      *s = 0.0;
      *h = NAN;
      return;
  }
  if( r >= max ) {                 // > is bogus, just keeps compiler happy
      *h = (g - b) / delta;        // between yellow & magenta
  } else if(g >= max) {
      *h = 2.0 + (b - r) / delta;  // between cyan & yellow
  } else {
      *h = 4.0 + (r - g) / delta;  // between magenta & cyan
  }

//    *h *= 60.0;                              // degrees
//    if(*h < 0.0 ) *h += 360.0;
  *h *= 100.0 / 6.0;
  if (*h < 0.0) *h += 100.0;

  return;
}


__global__ void HSLSelectKernel(
    int p_Width, int p_Height,
    bool _hueEnabled, float _hue, float _hueWidth, float _hueSoftness,
    bool _saturationEnabled, float _saturationLow, float _saturationHigh, float _saturationLowSoftness, float _saturationHighSoftness,
    bool _luminanceEnabled, float _luminanceLow, float _luminanceHigh, float _luminanceLowSoftness, float _luminanceHighSoftness,
    const float* p_Input, float* p_Output)
{
    // rgb are 0->1, return hsl as 0->100
    // Copied from the CPU version, dunno if it's optimal for GPU
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    float r, g, b;
    double h, s, l;
    double minHue, maxHue, overflowed_h, underflowed_h;
    double hue_multiplier, sat_multiplier, lum_multiplier;
    double hue_lower_softness_threshold, hue_upper_softness_threshold;
    minHue = _hue - .5 * _hueWidth;
    maxHue = _hue + .5 * _hueWidth;
    hue_lower_softness_threshold = minHue - _hueSoftness;
    hue_upper_softness_threshold = maxHue + _hueSoftness;

   if ((x < p_Width) && (y < p_Height))
   {
        const int index = ((y * p_Width) + x) * 4;
        r = p_Input[index + 0];
        g = p_Input[index + 1];
        b = p_Input[index + 2];
        rgb2hslcuda(
            r, g, b,
            &h, &s, &l
        );


        if (_hueEnabled) {
            overflowed_h = h - 100.0;  // "wrapped around" hue, for testing against negative softness window
            underflowed_h = h + 100.0; // "wrapped around" hue, for testing against overflowed softness window
            if (h >= minHue && h <= maxHue) {
                hue_multiplier = 1.0;
            } else if (overflowed_h >= minHue && overflowed_h <= maxHue) {
                hue_multiplier = 1.0;
            } else if (underflowed_h >= minHue && underflowed_h <= maxHue) {
                hue_multiplier = 1.0;
            } else if (h > hue_lower_softness_threshold && h < minHue) {
                hue_multiplier = (h - hue_lower_softness_threshold) / _hueSoftness;
            } else if (overflowed_h > hue_lower_softness_threshold && overflowed_h < minHue) {
                hue_multiplier = (overflowed_h - hue_lower_softness_threshold) / _hueSoftness;
            } else if (h > maxHue && h <= hue_upper_softness_threshold) {
                hue_multiplier = (hue_upper_softness_threshold - h) / _hueSoftness;
            } else if (underflowed_h > maxHue && underflowed_h <= hue_upper_softness_threshold) {
                hue_multiplier = (hue_upper_softness_threshold - underflowed_h) / _hueSoftness;
            } else {
                hue_multiplier = 0.0;
            }
        } else hue_multiplier = 1.0;
        //if (cc<1) printf("%f %f %f min=%f max=%f a=%f\n", h, s, v, minHue, maxHue, a);
        //if (cc<1) printf("%f %f %f min=%f max=%f\n", h, s, l, luminance_low, luminance_high);

        if (_saturationEnabled) {
            if (s >= _saturationLow && s <= _saturationHigh) {
                sat_multiplier = 1.0;
            } else if (s < _saturationLow && s > _saturationLow - _saturationLowSoftness) {
                sat_multiplier = (s - (_saturationLow - _saturationLowSoftness)) / _saturationLowSoftness;
            } else if (s > _saturationHigh && s < _saturationHigh + _saturationHighSoftness){
                sat_multiplier = 1.0 - (s - _saturationHigh) / _saturationHighSoftness;
            } else {
                sat_multiplier = 0.0;
            }
        } else sat_multiplier = 1.0;

        if (_luminanceEnabled) {
            if (l >= _luminanceLow && l <= _luminanceHigh) {
                lum_multiplier = 1.0;
            } else if (l < _luminanceLow && l > _luminanceLow - _luminanceLowSoftness) {
                lum_multiplier = (l - (_luminanceLow - _luminanceLowSoftness)) / _luminanceLowSoftness;
            } else if (s > _luminanceHigh && l < _luminanceHigh + _luminanceHighSoftness){
                lum_multiplier = 1.0 - (l - _luminanceHigh) / _luminanceHighSoftness;
            } else {
                lum_multiplier = 0.0;
            }
        } else lum_multiplier = 1.0;
        //if (cc<1) printf("lum multiplier=%f\n", lum_multiplier);


        p_Output[index + 0] = r;
        p_Output[index + 1] = g;
        p_Output[index + 2] = b;
        p_Output[index + 3] = hue_multiplier * sat_multiplier * lum_multiplier;
    }
}

void RunCudaKernel(
    void* p_Stream, int p_Width, int p_Height,
    bool hueEnabled, float hue, float hueWidth, float hueSoftness,
    bool saturationEnabled, float saturationLow, float saturationHigh, float saturationLowSoftness, float saturationHighSoftness,
    bool luminanceEnabled, float luminanceLow, float luminanceHigh, float luminanceLowSoftness, float luminanceHighSoftness,
    const float* p_Input, float* p_Output)
{
    dim3 threads(128, 1, 1);
    dim3 blocks(((p_Width + threads.x - 1) / threads.x), p_Height, 1);
    cudaStream_t stream = static_cast<cudaStream_t>(p_Stream);

    HSLSelectKernel<<<blocks, threads, 0, stream>>>(
        p_Width, p_Height,
        hueEnabled, hue, hueWidth, hueSoftness,
        saturationEnabled, saturationLow, saturationHigh, saturationLowSoftness, saturationHighSoftness,
        luminanceEnabled, luminanceLow, luminanceHigh, luminanceLowSoftness, luminanceHighSoftness,
        p_Input, p_Output
    );
}

