#include "qualiflower.h"

#include <stdio.h>
#include <math.h>

#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"
#include "ofxsProcessing.h"
#include "ofxsLog.h"

#define kPluginName "QualiFlower"
#define kPluginGrouping "Matte"
#define kPluginDescription "Make selections based on image hue, saturation and luminance"
#define kPluginIdentifier "joeboy.QualiFlower"
#define kPluginVersionMajor 0
#define kPluginVersionMinor 2

#define kSupportsTiles false
#define kSupportsMultiResolution false
#define kSupportsMultipleClipPARs false

////////////////////////////////////////////////////////////////////////////////

class ImageScaler : public OFX::ImageProcessor
{
public:
    explicit ImageScaler(OFX::ImageEffect& p_Instance);

    virtual void processImagesCUDA();
    virtual void multiThreadProcessImages(OfxRectI p_ProcWindow);

    void setSrcImg(OFX::Image* p_SrcImg);
    void setParams(
        bool p_hueEnabled, float p_hue, float p_hueWidth, float p_hueSoftness,
        bool p_saturationEnabled, float p_saturationLow, float p_saturationHigh, float p_saturationLowSoftness, float p_saturationHighSoftness,
        bool p_luminanceEnabled, float p_luminanceLow, float p_luminanceHigh, float p_luminanceLowSoftness, float p_luminanceHighSoftness
    );



private:
    OFX::Image* _srcImg;
    bool _hueEnabled, _saturationEnabled, _luminanceEnabled;
    float _hue, _hueWidth, _hueSoftness;
    float _saturationLow, _saturationHigh, _saturationLowSoftness, _saturationHighSoftness;
    float _luminanceLow, _luminanceHigh, _luminanceLowSoftness, _luminanceHighSoftness;
};

ImageScaler::ImageScaler(OFX::ImageEffect& p_Instance)
    : OFX::ImageProcessor(p_Instance)
{
}

#ifndef __APPLE__
extern void RunCudaKernel(
    void* p_Stream, int p_Width, int p_Height,
    bool hueEnabled, float hue, float hueWidth, float hueSoftness,
    bool saturationEnabled, float saturationLow, float saturationHigh, float saturationLowSoftness, float saturationHighSoftness,
    bool luminanceEnabled, float luminanceLow, float luminanceHigh, float luminanceLowSoftness, float luminanceHighSoftness,
    const float* p_Input, float* p_Output
);
#endif

void ImageScaler::processImagesCUDA()
{
#ifndef __APPLE__
    const OfxRectI& bounds = _srcImg->getBounds();
    const int width = bounds.x2 - bounds.x1;
    const int height = bounds.y2 - bounds.y1;

    float* input = static_cast<float*>(_srcImg->getPixelData());
    float* output = static_cast<float*>(_dstImg->getPixelData());

    RunCudaKernel(
        _pCudaStream, width, height,
        _hueEnabled, _hue, _hueWidth, _hueSoftness,
        _saturationEnabled, _saturationLow, _saturationHigh, _saturationLowSoftness, _saturationHighSoftness,
        _luminanceEnabled, _luminanceLow, _luminanceHigh, _luminanceLowSoftness, _luminanceHighSoftness,
        input, output
    );
#endif
}


void rgb2hsl(double r, double g, double b, double *h, double *s, double *l)
{
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


void ImageScaler::multiThreadProcessImages(OfxRectI p_ProcWindow)
{
    double minHue, maxHue, overflowed_h, underflowed_h;
    minHue = _hue - .5 * _hueWidth;
    maxHue = _hue + .5 * _hueWidth;

    int cc=0;
    // and do some processing
    double h, s, l;
    double hue_multiplier, sat_multiplier, lum_multiplier;
    double hue_lower_softness_threshold, hue_upper_softness_threshold;
    hue_lower_softness_threshold = minHue - _hueSoftness;
    hue_upper_softness_threshold = maxHue + _hueSoftness;

    for(int y = p_ProcWindow.y1; y < p_ProcWindow.y2; y++) {
        //if(y % 20 == 0 && gImageEffectSuite->abort(instance)) break;
        if (_effect.abort()) break;

        // get the row start for the output image
        float* dstPix = static_cast<float*>(_dstImg->getPixelAddress(p_ProcWindow.x1, y));

        for(int x = p_ProcWindow.x1; x < p_ProcWindow.x2; x++) {
            // get the source pixel
            float* srcPix = static_cast<float*>(_srcImg ? _srcImg->getPixelAddress(x, y) : 0);

            if(srcPix) {
                float r = srcPix[0];
                float g = srcPix[1];
                float b = srcPix[2];
                rgb2hsl(r, g, b, &h, &s, &l);

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

                dstPix[0] = r;
                dstPix[1] = g;
                dstPix[2] = b;
                dstPix[3] = hue_multiplier * sat_multiplier * lum_multiplier;
                srcPix += 4;
                dstPix += 4;
            } else {
                // we don't have a pixel in the source image, set output to zero
                for(int i = 0; i < 4; ++i) {
                    *dstPix = 0;
                    ++dstPix;
                }
            }
        }
        cc++;
    }
}

void ImageScaler::setSrcImg(OFX::Image* p_SrcImg)
{
    _srcImg = p_SrcImg;
}

void ImageScaler::setParams(
        bool p_hueEnabled, float p_hue, float p_hueWidth, float p_hueSoftness,
        bool p_saturationEnabled, float p_saturationLow, float p_saturationHigh, float p_saturationLowSoftness, float p_saturationHighSoftness,
        bool p_luminanceEnabled, float p_luminanceLow, float p_luminanceHigh, float p_luminanceLowSoftness, float p_luminanceHighSoftness
)
{
    _hueEnabled = p_hueEnabled;
    _hue = p_hue;
    _hueWidth = p_hueWidth;
    _hueSoftness = p_hueSoftness;
    _saturationEnabled = p_saturationEnabled;
    _saturationLow = p_saturationLow;
    _saturationHigh = p_saturationHigh;
    _saturationLowSoftness = p_saturationLowSoftness;
    _saturationHighSoftness = p_saturationHighSoftness;
    _luminanceEnabled = p_luminanceEnabled;
    _luminanceLow = p_luminanceLow;
    _luminanceHigh = p_luminanceHigh;
    _luminanceLowSoftness = p_luminanceLowSoftness;
    _luminanceHighSoftness = p_luminanceHighSoftness;
}


////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class QualiFlowerPlugin : public OFX::ImageEffect
{
public:
    explicit QualiFlowerPlugin(OfxImageEffectHandle p_Handle);

    /* Override the render */
    virtual void render(const OFX::RenderArguments& p_Args);

    /* Override is identity */
    virtual bool isIdentity(const OFX::IsIdentityArguments& p_Args, OFX::Clip*& p_IdentityClip, double& p_IdentityTime);

    /* Override changedParam */
    virtual void changedParam(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ParamName);

    /* Override changed clip */
    virtual void changedClip(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ClipName);

    /* Set the enabledness of the params depending on the type of input image and the state of the checkboxes */
    void setEnabledness();

    /* Set up and run a processor */
    void setupAndProcess(ImageScaler &p_ImageScaler, const OFX::RenderArguments& p_Args);

private:
    // Does not own the following pointers
    OFX::Clip* m_DstClip;
    OFX::Clip* m_SrcClip;

    OFX::DoubleParam* m_Scale;
    OFX::DoubleParam* m_ScaleR;
    OFX::DoubleParam* m_ScaleG;
    OFX::DoubleParam* m_ScaleB;
    OFX::DoubleParam* m_ScaleA;
    OFX::BooleanParam* m_ComponentScalesEnabled;

    OFX::BooleanParam* m_hueEnabled;
    OFX::DoubleParam* m_hue;
    OFX::DoubleParam* m_hueWidth;
    OFX::DoubleParam* m_hueSoftness;

    OFX::BooleanParam* m_saturationEnabled;
    OFX::DoubleParam* m_saturationLow;
    OFX::DoubleParam* m_saturationHigh;
    OFX::DoubleParam* m_saturationLowSoftness;
    OFX::DoubleParam* m_saturationHighSoftness;

    OFX::BooleanParam* m_luminanceEnabled;
    OFX::DoubleParam* m_luminanceLow;
    OFX::DoubleParam* m_luminanceHigh;
    OFX::DoubleParam* m_luminanceLowSoftness;
    OFX::DoubleParam* m_luminanceHighSoftness;

};

QualiFlowerPlugin::QualiFlowerPlugin(OfxImageEffectHandle p_Handle)
    : ImageEffect(p_Handle)
{
    m_DstClip = fetchClip(kOfxImageEffectOutputClipName);
    m_SrcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);

    m_hueEnabled = fetchBooleanParam("selectByHueEnabled");
    m_hue = fetchDoubleParam("hue");
    m_hueWidth = fetchDoubleParam("hueWidth");
    m_hueSoftness = fetchDoubleParam("hueSoftness");

    m_saturationEnabled = fetchBooleanParam("selectBySaturationEnabled");
    m_saturationLow = fetchDoubleParam("saturationLow");
    m_saturationHigh = fetchDoubleParam("saturationHigh");
    m_saturationLowSoftness = fetchDoubleParam("saturationLowSoftness");
    m_saturationHighSoftness = fetchDoubleParam("saturationHighSoftness");

    m_luminanceEnabled = fetchBooleanParam("selectByLuminanceEnabled");
    m_luminanceLow = fetchDoubleParam("luminanceLow");
    m_luminanceHigh = fetchDoubleParam("luminanceHigh");
    m_luminanceLowSoftness = fetchDoubleParam("luminanceLowSoftness");
    m_luminanceHighSoftness = fetchDoubleParam("luminanceHighSoftness");

    // Set the enabledness of our sliders
    setEnabledness();
}

void QualiFlowerPlugin::render(const OFX::RenderArguments& p_Args)
{
    if ((m_DstClip->getPixelDepth() == OFX::eBitDepthFloat) && (m_DstClip->getPixelComponents() == OFX::ePixelComponentRGBA))
    {
        ImageScaler imageScaler(*this);
        setupAndProcess(imageScaler, p_Args);
    }
    else
    {
        OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
    }
}

bool QualiFlowerPlugin::isIdentity(const OFX::IsIdentityArguments& p_Args, OFX::Clip*& p_IdentityClip, double& p_IdentityTime)
{
    bool hueEnabled = m_hueEnabled->getValueAtTime(p_Args.time);
    bool saturationEnabled = m_saturationEnabled->getValueAtTime(p_Args.time);
    bool luminanceEnabled = m_luminanceEnabled->getValueAtTime(p_Args.time);
    // TODO: Could also check for all 0-100% values here
    if (!hueEnabled && !saturationEnabled && !luminanceEnabled) {
         p_IdentityClip = m_SrcClip;
         p_IdentityTime = p_Args.time;
        return true;
    }
    return false;
}

void QualiFlowerPlugin::changedParam(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ParamName)
{
    if (
        p_ParamName == "selectByHueEnabled"
        || (p_ParamName == "selectBySaturationEnabled")
        || (p_ParamName == "selectByLuminanceEnabled")
    )
    {
        setEnabledness();
    }
}

void QualiFlowerPlugin::changedClip(const OFX::InstanceChangedArgs& p_Args, const std::string& p_ClipName)
{
    if (p_ClipName == kOfxImageEffectSimpleSourceClipName)
    {
        setEnabledness();
    }
}

void QualiFlowerPlugin::setEnabledness()
{
    // the param enabledness depends on the clip being RGBA and the param being true
    const bool enableHue = (m_hueEnabled->getValue() && (m_SrcClip->getPixelComponents() == OFX::ePixelComponentRGBA));
    const bool enableSaturation = (m_saturationEnabled->getValue() && (m_SrcClip->getPixelComponents() == OFX::ePixelComponentRGBA));
    const bool enableLuminance = (m_luminanceEnabled->getValue() && (m_SrcClip->getPixelComponents() == OFX::ePixelComponentRGBA));
    m_hue->setEnabled(enableHue);
    m_hueWidth->setEnabled(enableHue);
    m_hueSoftness->setEnabled(enableHue);
    m_saturationLow->setEnabled(enableSaturation);
    m_saturationHigh->setEnabled(enableSaturation);
    m_saturationLowSoftness->setEnabled(enableSaturation);
    m_saturationHighSoftness->setEnabled(enableSaturation);
    m_luminanceLow->setEnabled(enableLuminance);
    m_luminanceLowSoftness->setEnabled(enableLuminance);
    m_luminanceHigh->setEnabled(enableLuminance);
    m_luminanceHighSoftness->setEnabled(enableLuminance);
}

void QualiFlowerPlugin::setupAndProcess(ImageScaler& p_ImageScaler, const OFX::RenderArguments& p_Args)
{
    // Get the dst image
    std::auto_ptr<OFX::Image> dst(m_DstClip->fetchImage(p_Args.time));
    OFX::BitDepthEnum dstBitDepth = dst->getPixelDepth();
    OFX::PixelComponentEnum dstComponents = dst->getPixelComponents();

    // Get the src image
    std::auto_ptr<OFX::Image> src(m_SrcClip->fetchImage(p_Args.time));
    OFX::BitDepthEnum srcBitDepth = src->getPixelDepth();
    OFX::PixelComponentEnum srcComponents = src->getPixelComponents();

    // Check to see if the bit depth and number of components are the same
    if ((srcBitDepth != dstBitDepth) || (srcComponents != dstComponents))
    {
        OFX::throwSuiteStatusException(kOfxStatErrValue);
    }

    bool hueEnabled = m_hueEnabled->getValueAtTime(p_Args.time);
    float hue = m_hue->getValueAtTime(p_Args.time);
    float hueWidth = m_hueWidth->getValueAtTime(p_Args.time);
    float hueSoftness = m_hueSoftness->getValueAtTime(p_Args.time);
    bool saturationEnabled = m_saturationEnabled->getValueAtTime(p_Args.time);
    float saturationLow = m_saturationLow->getValueAtTime(p_Args.time);
    float saturationHigh = m_saturationHigh->getValueAtTime(p_Args.time);
    float saturationLowSoftness = m_saturationLowSoftness->getValueAtTime(p_Args.time);
    float saturationHighSoftness = m_saturationHighSoftness->getValueAtTime(p_Args.time);
    bool luminanceEnabled = m_luminanceEnabled->getValueAtTime(p_Args.time);
    float luminanceLow = m_luminanceLow->getValueAtTime(p_Args.time);
    float luminanceHigh = m_luminanceHigh->getValueAtTime(p_Args.time);
    float luminanceLowSoftness = m_luminanceLowSoftness->getValueAtTime(p_Args.time);
    float luminanceHighSoftness = m_luminanceHighSoftness->getValueAtTime(p_Args.time);

    // Set the images
    p_ImageScaler.setDstImg(dst.get());
    p_ImageScaler.setSrcImg(src.get());

    // Setup OpenCL and CUDA Render arguments
    p_ImageScaler.setGPURenderArgs(p_Args);

    // Set the render window
    p_ImageScaler.setRenderWindow(p_Args.renderWindow);

    p_ImageScaler.setParams(
        hueEnabled, hue, hueWidth, hueSoftness,
        saturationEnabled, saturationLow, saturationHigh, saturationLowSoftness, saturationHighSoftness,
        luminanceEnabled, luminanceLow, luminanceHigh, luminanceLowSoftness, luminanceHighSoftness
    );

    // Call the base class process member, this will call the derived templated process code
    p_ImageScaler.process();
}

////////////////////////////////////////////////////////////////////////////////

using namespace OFX;

QualiFlowerPluginFactory::QualiFlowerPluginFactory()
    : OFX::PluginFactoryHelper<QualiFlowerPluginFactory>(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor)
{
}

void QualiFlowerPluginFactory::describe(OFX::ImageEffectDescriptor& p_Desc)
{
    // Basic labels
    p_Desc.setLabels(kPluginName, kPluginName, kPluginName);
    p_Desc.setPluginGrouping(kPluginGrouping);
    p_Desc.setPluginDescription(kPluginDescription);

    // Add the supported contexts, only filter at the moment
    p_Desc.addSupportedContext(eContextFilter);
    p_Desc.addSupportedContext(eContextGeneral);

    // Add supported pixel depths
    p_Desc.addSupportedBitDepth(eBitDepthFloat);

    // Set a few flags
    p_Desc.setSingleInstance(false);
    p_Desc.setHostFrameThreading(false);
    p_Desc.setSupportsMultiResolution(kSupportsMultiResolution);
    p_Desc.setSupportsTiles(kSupportsTiles);
    p_Desc.setTemporalClipAccess(false);
    p_Desc.setRenderTwiceAlways(false);
    p_Desc.setSupportsMultipleClipPARs(kSupportsMultipleClipPARs);

    // Setup OpenCL render capability flags
    p_Desc.setSupportsOpenCLRender(false);

    // Setup CUDA render capability flags on non-Apple system
#ifndef __APPLE__
    p_Desc.setSupportsCudaRender(true);
    p_Desc.setSupportsCudaStream(true);
#endif

    // Setup Metal render capability flags only on Apple system
#ifdef __APPLE__
     p_Desc.setSupportsMetalRender(false);
#endif

    // Indicates that the plugin output does not depend on location or neighbours of a given pixel.
    // Therefore, this plugin could be executed during LUT generation.
    p_Desc.setNoSpatialAwareness(true);
}

static DoubleParamDescriptor* defineScaleParam(OFX::ImageEffectDescriptor& p_Desc, const std::string& p_Name, const std::string& p_Label,
                                               const std::string& p_Hint, GroupParamDescriptor* p_Parent)
{
    DoubleParamDescriptor* param = p_Desc.defineDoubleParam(p_Name);
    param->setLabels(p_Label, p_Label, p_Label);
    param->setScriptName(p_Name);
    param->setHint(p_Hint);
    param->setDefault(1);
    param->setRange(0, 100);
    param->setIncrement(1.0);
    param->setDisplayRange(0, 100);
    param->setDoubleType(eDoubleTypeScale);

    if (p_Parent)
    {
        param->setParent(*p_Parent);
    }

    return param;
}


void QualiFlowerPluginFactory::describeInContext(OFX::ImageEffectDescriptor& p_Desc, OFX::ContextEnum /*p_Context*/)
{
    // Source clip only in the filter context
    // Create the mandated source clip
    ClipDescriptor* srcClip = p_Desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(kSupportsTiles);
    srcClip->setIsMask(false);

    // Create the mandated output clip
    ClipDescriptor* dstClip = p_Desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentAlpha);
    dstClip->setSupportsTiles(kSupportsTiles);

    // Make some pages and to things in
    PageParamDescriptor* page = p_Desc.definePageParam("Controls");

    // Group param to group the scales
    GroupParamDescriptor* selectionGroup = p_Desc.defineGroupParam("Selections");
    selectionGroup->setHint("HSL selection");
    selectionGroup->setLabels("Selection", "Selection", "Selection");

    DoubleParamDescriptor* param;
    BooleanParamDescriptor* boolParam;
    boolParam = p_Desc.defineBooleanParam("selectByHueEnabled");
    boolParam->setDefault(true);
    boolParam->setHint("Enable selection by hue");
    boolParam->setLabels("Select by Hue", "Select by Hue", "Select by Hue");
    boolParam->setParent(*selectionGroup);
    page->addChild(*boolParam);
    param = defineScaleParam(p_Desc, "hue", "Hue", "Hue selection", selectionGroup);
    page->addChild(*param);
    param = defineScaleParam(p_Desc, "hueWidth", "Hue Width", "Hue width", selectionGroup);
    page->addChild(*param);
    param = defineScaleParam(p_Desc, "hueSoftness", "Hue Softness", "Hue softness", selectionGroup);
    page->addChild(*param);

    boolParam = p_Desc.defineBooleanParam("selectBySaturationEnabled");
    boolParam->setDefault(true);
    boolParam->setHint("Enable selection by saturation");
    boolParam->setLabels("Select by Saturation", "Select by Saturation", "Select by Saturation");
    boolParam->setParent(*selectionGroup);
    page->addChild(*boolParam);
    param = defineScaleParam(p_Desc, "saturationLow", "Saturation Low", "Saturation Low", selectionGroup);
    page->addChild(*param);
    param = defineScaleParam(p_Desc, "saturationHigh", "Saturation High", "Saturation High", selectionGroup);
    page->addChild(*param);
    param = defineScaleParam(p_Desc, "saturationLowSoftness", "Saturation Low Softness", "Saturation Low softness", selectionGroup);
    page->addChild(*param);
    param = defineScaleParam(p_Desc, "saturationHighSoftness", "Saturation High Softness", "Saturation High softness", selectionGroup);
    page->addChild(*param);

    boolParam = p_Desc.defineBooleanParam("selectByLuminanceEnabled");
    boolParam->setDefault(true);
    boolParam->setHint("Enable selection by luminance");
    boolParam->setLabels("Select by Luminance", "Select by Luminance", "Select by Luminance");
    boolParam->setParent(*selectionGroup);
    page->addChild(*boolParam);
    param = defineScaleParam(p_Desc, "luminanceLow", "Luminance Low", "Luminance Low", selectionGroup);
    page->addChild(*param);
    param = defineScaleParam(p_Desc, "luminanceHigh", "Luminance High", "Luminance High", selectionGroup);
    page->addChild(*param);
    param = defineScaleParam(p_Desc, "luminanceLowSoftness", "Luinance Low Softness", "Luminance Low softness", selectionGroup);
    page->addChild(*param);
    param = defineScaleParam(p_Desc, "luminanceHighSoftness", "Luminance High Softness", "Luminance High softness", selectionGroup);
    page->addChild(*param);
}

ImageEffect* QualiFlowerPluginFactory::createInstance(OfxImageEffectHandle p_Handle, ContextEnum /*p_Context*/)
{
    return new QualiFlowerPlugin(p_Handle);
}

void OFX::Plugin::getPluginIDs(PluginFactoryArray& p_FactoryArray)
{
    static QualiFlowerPluginFactory qualiflowerPlugin;
    p_FactoryArray.push_back(&qualiflowerPlugin);
}
