#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <string>
#include <iostream>

// the one OFX header we need, it includes the others necessary
#include "ofxImageEffect.h"

#if defined __APPLE__ || defined linux
#  define EXPORT __attribute__((visibility("default")))
#elif defined _WIN32
#  define EXPORT OfxExport
#else
#  error Not building on your operating system quite yet
#endif

////////////////////////////////////////////////////////////////////////////////
// macro to write a labelled message to stderr with
#ifdef _WIN32
  #define DUMP(LABEL, MSG, ...)                                           \
  {                                                                       \
    fprintf(stderr, "%s%s:%d in %s ", LABEL, __FILE__, __LINE__, __FUNCTION__); \
    fprintf(stderr, MSG, ##__VA_ARGS__);                                  \
    fprintf(stderr, "\n");                                                \
  }
#else
  #define DUMP(LABEL, MSG, ...)                                           \
  {                                                                       \
    fprintf(stderr, "%s%s:%d in %s ", LABEL, __FILE__, __LINE__, __PRETTY_FUNCTION__); \
    fprintf(stderr, MSG, ##__VA_ARGS__);                                  \
    fprintf(stderr, "\n");                                                \
  }
#endif

// macro to write a simple message, only works if 'VERBOSE' is #defined
//#define VERBOSE
#ifdef VERBOSE
#  define MESSAGE(MSG, ...) DUMP("", MSG, ##__VA_ARGS__)
#else
#  define MESSAGE(MSG, ...)
#endif

// macro to dump errors to stderr if the given condition is true
#define ERROR_IF(CONDITION, MSG, ...) if(CONDITION) { DUMP("ERROR : ", MSG, ##__VA_ARGS__);}

// macro to dump errors to stderr and abort if the given condition is true
#define ERROR_ABORT_IF(CONDITION, MSG, ...)     \
{                                               \
  if(CONDITION) {                               \
    DUMP("FATAL ERROR : ", MSG, ##__VA_ARGS__); \
    abort();                                    \
  }                                             \
}

// name of our two params
#define HUE_ENABLED_PARAM_NAME "hueEnabled"
#define HUE_PARAM_NAME "hue"
#define HUE_WIDTH_PARAM_NAME "hueWidth"
#define HUE_SOFTNESS_PARAM_NAME "hueSoftness"
#define SATURATION_ENABLED_PARAM_NAME "saturationEnabled"
#define SATURATION_LOW_PARAM_NAME "saturationLow"
#define SATURATION_HIGH_PARAM_NAME "saturationHigh"
#define SATURATION_LOW_SOFTNESS_PARAM_NAME "saturationLowSoftness"
#define SATURATION_HIGH_SOFTNESS_PARAM_NAME "saturationHighSoftness"
#define LUMINANCE_ENABLED_PARAM_NAME "luminanceEnabled"
#define LUMINANCE_LOW_PARAM_NAME "luminanceLow"
#define LUMINANCE_HIGH_PARAM_NAME "luminanceHigh"
#define LUMINANCE_LOW_SOFTNESS_PARAM_NAME "luminanceLowSoftness"
#define LUMINANCE_HIGH_SOFTNESS_PARAM_NAME "luminanceHighSoftness"

// anonymous namespace to hide our symbols in
namespace {
  ////////////////////////////////////////////////////////////////////////////////
  // set of suite pointers provided by the host
  OfxHost               *gHost;
  OfxPropertySuiteV1    *gPropertySuite    = 0;
  OfxImageEffectSuiteV1 *gImageEffectSuite = 0;
  OfxParameterSuiteV1   *gParameterSuite   = 0;

  ////////////////////////////////////////////////////////////////////////////////
  // our instance data, where we are caching away clip and param handles
  struct MyInstanceData {
    // handles to the clips we deal with
    OfxImageClipHandle sourceClip;
    OfxImageClipHandle outputClip;

    // handles to a our parameters
    OfxParamHandle hueEnabledParam;
    OfxParamHandle hueParam;
    OfxParamHandle hueWidthParam;
    OfxParamHandle hueSoftnessParam;
    OfxParamHandle saturationEnabledParam;
    OfxParamHandle saturationLowParam;
    OfxParamHandle saturationHighParam;
    OfxParamHandle saturationLowSoftnessParam;
    OfxParamHandle saturationHighSoftnessParam;
    OfxParamHandle luminanceEnabledParam;
    OfxParamHandle luminanceLowParam;
    OfxParamHandle luminanceHighParam;
    OfxParamHandle luminanceLowSoftnessParam;
    OfxParamHandle luminanceHighSoftnessParam;
  };

  ////////////////////////////////////////////////////////////////////////////////
  // get my instance data from a property set handle
  MyInstanceData *FetchInstanceData(OfxPropertySetHandle effectProps)
  {
    MyInstanceData *myData = 0;
    gPropertySuite->propGetPointer(effectProps,
                                   kOfxPropInstanceData,
                                   0,
                                   (void **) &myData);
    return myData;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // get my instance data
  MyInstanceData *FetchInstanceData(OfxImageEffectHandle effect)
  {
    // get the property handle for the plugin
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(effect, &effectProps);

    // and get the instance data out of that
    return FetchInstanceData(effectProps);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // get the named suite and put it in the given pointer, with error checking
  template <class SUITE>
  void FetchSuite(SUITE *& suite, const char *suiteName, int suiteVersion)
  {
    suite = (SUITE *) gHost->fetchSuite(gHost->host, suiteName, suiteVersion);
    if(!suite) {
      ERROR_ABORT_IF(suite == NULL,
                     "Failed to fetch %s version %d from the host.",
                     suiteName,
                     suiteVersion);
    }
  }

  ////////////////////////////////////////////////////////////////////////////////
  // The first _action_ called after the binary is loaded (three boot strapper functions will be howeever)
  OfxStatus LoadAction(void)
  {
    // fetch our three suites
    FetchSuite(gPropertySuite,    kOfxPropertySuite,    1);
    FetchSuite(gImageEffectSuite, kOfxImageEffectSuite, 1);
    FetchSuite(gParameterSuite,   kOfxParameterSuite,   1);

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // the plugin's basic description routine
  OfxStatus DescribeAction(OfxImageEffectHandle descriptor)
  {
    // get the property set handle for the plugin
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(descriptor, &effectProps);

    // set some labels and the group it belongs to
    gPropertySuite->propSetString(effectProps,
                                  kOfxPropLabel,
                                  0,
                                  "QualiFlower");
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPluginPropGrouping,
                                  0,
                                  "Mask");

    // define the image effects contexts we can be used in, in this case a simple filter
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedContexts,
                                  0,
                                  kOfxImageEffectContextFilter);

    // set the bit depths the plugin can handle
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedPixelDepths,
                                  0,
                                  kOfxBitDepthFloat);
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedPixelDepths,
                                  1,
                                  kOfxBitDepthShort);
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPropSupportedPixelDepths,
                                  2,
                                  kOfxBitDepthByte);

    // say that a single instance of this plugin can be rendered in multiple threads
    gPropertySuite->propSetString(effectProps,
                                  kOfxImageEffectPluginRenderThreadSafety,
                                  0,
                                  kOfxImageEffectRenderFullySafe);

    // say that the host should manage SMP threading over a single frame
    gPropertySuite->propSetInt(effectProps,
                               kOfxImageEffectPluginPropHostFrameThreading,
                               0,
                               1);


    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  //  describe the plugin in context
  OfxStatus
  DescribeInContextAction(OfxImageEffectHandle descriptor,
                          OfxPropertySetHandle inArgs)
  {
    OfxPropertySetHandle props;
    // define the mandated single output clip
    gImageEffectSuite->clipDefine(descriptor, "Output", &props);

    // set the component types we can handle on out output
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  0,
                                  kOfxImageComponentRGBA);
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  1,
                                  kOfxImageComponentAlpha);
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  2,
                                  kOfxImageComponentRGB);

    // define the mandated single source clip
    gImageEffectSuite->clipDefine(descriptor, "Source", &props);

    // set the component types we can handle on our main input
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  0,
                                  kOfxImageComponentRGBA);
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  1,
                                  kOfxImageComponentAlpha);
    gPropertySuite->propSetString(props,
                                  kOfxImageEffectPropSupportedComponents,
                                  2,
                                  kOfxImageComponentRGB);

    // first get the handle to the parameter set
    OfxParamSetHandle paramSet;
    gImageEffectSuite->getParamSet(descriptor, &paramSet);

    // properties on our parameter
    OfxPropertySetHandle paramProps;

    // "Hue bypass" parameter
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeBoolean,
                                 HUE_ENABLED_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetInt(paramProps,
                               kOfxParamPropDefault,
                               0,
                               0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Enable selection by hue.");
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Select by Hue");


    // 'hue' parameter and properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 HUE_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDefault,
                                  0,
                                  50.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Hue");
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Hue selection.");

    // now define a 'hueWidth' parameter and set its properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 HUE_WIDTH_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDefault,
                                  0,
                                  10.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Hue Width");
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Width of hue selection.");

    // now define a 'hue softness' parameter and set its properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 HUE_SOFTNESS_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDefault,
                                  0,
                                  5);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMax,
                                  0,
                                  50.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  50.0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Hue Softness");
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Softness of hue selection.");

    // "Saturation bypass" parameter
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeBoolean,
                                 SATURATION_ENABLED_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetInt(paramProps,
                               kOfxParamPropDefault,
                               0,
                               0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Enable selection by Saturation.");
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Select by Saturation");

    // now define a 'saturation low' parameter and set its properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 SATURATION_LOW_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDefault,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Saturation Low");
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Saturation Low selection.");

    // now define a 'saturation high' parameter and set its properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 SATURATION_HIGH_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDefault,
                                  0,
                                  100.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Saturation High");
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Saturation High selection.");

    // now define a 'saturation low softness' parameter and set its properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 SATURATION_LOW_SOFTNESS_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDefault,
                                  0,
                                  10.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Saturation Low Softness");
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Saturation Low Softness selection.");

    // now define a 'saturation high softness' parameter and set its properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 SATURATION_HIGH_SOFTNESS_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDefault,
                                  0,
                                  10.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Saturation High Softness");
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Saturation High Softness selection.");

    // "Luminance bypass" parameter
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeBoolean,
                                 LUMINANCE_ENABLED_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetInt(paramProps,
                               kOfxParamPropDefault,
                               0,
                               0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Enable selection by Luminance.");
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Select by luminance");

    // now define a 'luminance low' parameter and set its properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 LUMINANCE_LOW_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDefault,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Luminance Low");
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Luminance Low selection.");

    // now define a 'luminance high' parameter and set its properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 LUMINANCE_HIGH_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDefault,
                                  0,
                                  100.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Luminance High");
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Luminance High selection.");

    // now define a 'luminance low softness' parameter and set its properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 LUMINANCE_LOW_SOFTNESS_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDefault,
                                  0,
                                  10.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Luminance Low Softness");
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Luminance Low Softness selection.");

    // 'luminance high softness' parameter and properties
    gParameterSuite->paramDefine(paramSet,
                                 kOfxParamTypeDouble,
                                 LUMINANCE_HIGH_SOFTNESS_PARAM_NAME,
                                 &paramProps);
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropDoubleType,
                                  0,
                                  kOfxParamDoubleTypeScale);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDefault,
                                  0,
                                  10.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMin,
                                  0,
                                  0.0);
    gPropertySuite->propSetDouble(paramProps,
                                  kOfxParamPropDisplayMax,
                                  0,
                                  100.0);
    gPropertySuite->propSetString(paramProps,
                                  kOfxPropLabel,
                                  0,
                                  "Luminance High Softness");
    gPropertySuite->propSetString(paramProps,
                                  kOfxParamPropHint,
                                  0,
                                  "Luminance High Softness selection.");

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// instance construction
  OfxStatus CreateInstanceAction( OfxImageEffectHandle instance)
  {
    OfxPropertySetHandle effectProps;
    gImageEffectSuite->getPropertySet(instance, &effectProps);

    // To avoid continual lookup, put our handles into our instance
    // data, those handles are guaranteed to be valid for the duration
    // of the instance.
    MyInstanceData *myData = new MyInstanceData;

    // Set my private instance data
    gPropertySuite->propSetPointer(effectProps, kOfxPropInstanceData, 0, (void *) myData);

    // Cache the source and output clip handles
    gImageEffectSuite->clipGetHandle(instance, "Source", &myData->sourceClip, 0);
    gImageEffectSuite->clipGetHandle(instance, "Output", &myData->outputClip, 0);

    // Cache away the param handles
    OfxParamSetHandle paramSet;
    gImageEffectSuite->getParamSet(instance, &paramSet);
    gParameterSuite->paramGetHandle(paramSet,
                                    HUE_ENABLED_PARAM_NAME,
                                    &myData->hueEnabledParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    HUE_PARAM_NAME,
                                    &myData->hueParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    HUE_WIDTH_PARAM_NAME,
                                    &myData->hueWidthParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    HUE_SOFTNESS_PARAM_NAME,
                                    &myData->hueSoftnessParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    SATURATION_ENABLED_PARAM_NAME,
                                    &myData->saturationEnabledParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    SATURATION_LOW_PARAM_NAME,
                                    &myData->saturationLowParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    SATURATION_HIGH_PARAM_NAME,
                                    &myData->saturationHighParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    SATURATION_LOW_SOFTNESS_PARAM_NAME,
                                    &myData->saturationLowSoftnessParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    SATURATION_HIGH_SOFTNESS_PARAM_NAME,
                                    &myData->saturationHighSoftnessParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    LUMINANCE_ENABLED_PARAM_NAME,
                                    &myData->luminanceEnabledParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    LUMINANCE_LOW_PARAM_NAME,
                                    &myData->luminanceLowParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    LUMINANCE_HIGH_PARAM_NAME,
                                    &myData->luminanceHighParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    LUMINANCE_LOW_SOFTNESS_PARAM_NAME,
                                    &myData->luminanceLowSoftnessParam,
                                    0);
    gParameterSuite->paramGetHandle(paramSet,
                                    LUMINANCE_HIGH_SOFTNESS_PARAM_NAME,
                                    &myData->luminanceHighSoftnessParam,
                                    0);

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // instance destruction
  OfxStatus DestroyInstanceAction( OfxImageEffectHandle instance)
  {
    // get my instance data
    MyInstanceData *myData = FetchInstanceData(instance);
    delete myData;

    return kOfxStatOK;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Look up a pixel in the image. returns null if the pixel was not
  // in the bounds of the image
  template <class T>
  static inline T * pixelAddress(int x, int y,
                                 void *baseAddress,
                                 OfxRectI bounds,
                                 int rowBytes,
                                 int nCompsPerPixel)
  {
    // Inside the bounds of this image?
    if(x < bounds.x1 || x >= bounds.x2 || y < bounds.y1 || y >= bounds.y2)
      return NULL;

    // turn image plane coordinates into offsets from the bottom left
    int yOffset = y - bounds.y1;
    int xOffset = x - bounds.x1;

    // Find the start of our row, using byte arithmetic
    void *rowStartAsVoid = reinterpret_cast<char *>(baseAddress) + yOffset * rowBytes;

    // turn the row start into a pointer to our data type
    T *rowStart = reinterpret_cast<T *>(rowStartAsVoid);

    // finally find the position of the first component of column
    return rowStart + (xOffset * nCompsPerPixel);
  }


  void rgb2hsl(double r, double g, double b, double *h, double *s, double *l)
  {
    // rgb are 0->1, return hsl as 0->100
    double min, max, delta;

    min = r < g ? r : g;
    min = min  < b ? min : b;

    max = r > g ? r : g;
    max = max  > b ? max  : b;

    // For some reason max can be (slightly) > 1
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
    if( r >= max ) {                          // > is bogus, just keeps compiler happy
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




  ////////////////////////////////////////////////////////////////////////////////
  // iterate over our pixels and process them
  template <class T, int MAX>
  void PixelProcessing(
                       int hue_enabled,
                       double hue,
                       double hue_width,
                       double hue_softness,
                       int saturation_enabled,
                       double saturation_low,
                       double saturation_high,
                       double saturation_low_softness,
                       double saturation_high_softness,
                       int luminance_enabled,
                       double luminance_low,
                       double luminance_high,
                       double luminance_low_softness,
                       double luminance_high_softness,
                       OfxImageEffectHandle instance,
                       OfxPropertySetHandle sourceImg,
                       OfxPropertySetHandle outputImg,
                       OfxRectI renderWindow,
                       int nComps)
  {
    printf("PixelProcessing\n");
    // fetch output image info from the property handle
    int dstRowBytes;
    OfxRectI dstBounds;
    void *dstPtr = NULL;
    gPropertySuite->propGetInt(outputImg, kOfxImagePropRowBytes, 0, &dstRowBytes);
    gPropertySuite->propGetIntN(outputImg, kOfxImagePropBounds, 4, &dstBounds.x1);
    gPropertySuite->propGetPointer(outputImg, kOfxImagePropData, 0, &dstPtr);

    if(dstPtr == NULL) {
      throw "Bad destination pointer";
    }

    // fetch input image info from the property handle
    int srcRowBytes;
    OfxRectI srcBounds;
    void *srcPtr = NULL;
    gPropertySuite->propGetInt(sourceImg, kOfxImagePropRowBytes, 0, &srcRowBytes);
    gPropertySuite->propGetIntN(sourceImg, kOfxImagePropBounds, 4, &srcBounds.x1);
    gPropertySuite->propGetPointer(sourceImg, kOfxImagePropData, 0, &srcPtr);

    if(srcPtr == NULL) {
      throw "Bad source pointer";
    }
    printf("hue=%f\n", hue);
    double minHue, maxHue;
    minHue = hue - .5 * hue_width;
    maxHue = hue + .5 * hue_width;
    printf("hue=%lf minHue=%lf maxHue=%lf\n", hue, minHue, maxHue);
    printf("saturation_low=%f saturation_high=%f maxHue=%f\n", saturation_low, saturation_high, maxHue);


    int cc=0;
    // and do some processing
      double h, s, l;
      double hue_multiplier, sat_multiplier, lum_multiplier;
    for(int y = renderWindow.y1; y < renderWindow.y2; y++) {
      if(y % 20 == 0 && gImageEffectSuite->abort(instance)) break;

      // get the row start for the output image
      T *dstPix = pixelAddress<T>(renderWindow.x1, y,
                                  dstPtr,
                                  dstBounds,
                                  dstRowBytes,
                                  nComps);

      for(int x = renderWindow.x1; x < renderWindow.x2; x++) {

        // get the source pixel
        T *srcPix = pixelAddress<T>(x, y,
                                    srcPtr,
                                    srcBounds,
                                    srcRowBytes,
                                    nComps);


        if(srcPix) {
          T r = srcPix[0];
          T g = srcPix[1];
          T b = srcPix[2];
          rgb2hsl(r, g, b, &h, &s, &l);

          if (hue_enabled) {
            if (h >= minHue && h <= maxHue) {
              hue_multiplier = 1.0;
            } else if (h < minHue && h > minHue - hue_softness) {
              hue_multiplier = (h - (minHue - hue_softness)) / hue_softness;
            } else if (h > maxHue && h <= maxHue + hue_softness) {
              hue_multiplier = 1.0 - ((h - maxHue) / hue_softness);
            } else {
              hue_multiplier = 0.0;
            }
          } else hue_multiplier = 1.0;
          //if (cc<1) printf("%f %f %f min=%f max=%f a=%f\n", h, s, v, minHue, maxHue, a);
          //if (cc<1) printf("%f %f %f min=%f max=%f\n", h, s, l, luminance_low, luminance_high);

          if (saturation_enabled) {
            if (s >= saturation_low && s <= saturation_high) {
              sat_multiplier = 1.0;
            } else if (s < saturation_low && s > saturation_low - saturation_low_softness) {
              sat_multiplier = (s - (saturation_low - saturation_low_softness)) / saturation_low_softness;
            } else if (s > saturation_high && s < saturation_high + saturation_high_softness){
              sat_multiplier = 1.0 - (s - saturation_high) / saturation_high_softness;
            } else {
              sat_multiplier = 0.0;
            }
          } else sat_multiplier = 1.0;

          if (luminance_enabled) {
            if (l >= luminance_low && l <= luminance_high) {
              lum_multiplier = 1.0;
            } else if (l < luminance_low && l > luminance_low - luminance_low_softness) {
              lum_multiplier = (l - (luminance_low - luminance_low_softness)) / luminance_low_softness;
            } else if (s > luminance_high && l < luminance_high + luminance_high_softness){
              lum_multiplier = 1.0 - (l - luminance_high) / luminance_high_softness;
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
          for(int i = 0; i < nComps; ++i) {
            *dstPix = 0;
            ++dstPix;
          }
        }
      }
      cc++;
    }
  }


  ////////////////////////////////////////////////////////////////////////////////
  // Render an output image
  OfxStatus RenderAction( OfxImageEffectHandle instance,
                          OfxPropertySetHandle inArgs,
                          OfxPropertySetHandle outArgs)
  {
    printf("Render start\n");
    // get the render window and the time from the inArgs
    OfxTime time;
    OfxRectI renderWindow;
    OfxStatus status = kOfxStatOK;

    gPropertySuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    gPropertySuite->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

    // get our instance data which has out clip and param handles
  printf("16\n");
    MyInstanceData *myData = FetchInstanceData(instance);

    printf("17\n");
    // get our param values
    int hue_enabled = 0;
    double hue = 50.0;
    double hue_width = 100.0;
    double hue_softness = 10.0;
    int saturation_enabled = 0;
    double saturation_low = 0.0;
    double saturation_high = 100.0;
    double saturation_low_softness = 10.0;
    double saturation_high_softness = 10.0;
    int luminance_enabled = 0;
    double luminance_low = 0.0;
    double luminance_high = 100.0;
    double luminance_low_softness = 10.0;
    double luminance_high_softness = 10.0;
    gParameterSuite->paramGetValueAtTime(myData->hueEnabledParam, time, &hue_enabled);
    gParameterSuite->paramGetValueAtTime(myData->saturationEnabledParam, time, &saturation_enabled);
    gParameterSuite->paramGetValueAtTime(myData->luminanceEnabledParam, time, &luminance_enabled);
    gParameterSuite->paramGetValueAtTime(myData->hueParam, time, &hue);
    gParameterSuite->paramGetValueAtTime(myData->hueWidthParam, time, &hue_width);
    gParameterSuite->paramGetValueAtTime(myData->hueSoftnessParam, time, &hue_softness);
    gParameterSuite->paramGetValueAtTime(myData->saturationLowParam, time, &saturation_low);
    gParameterSuite->paramGetValueAtTime(myData->saturationHighParam, time, &saturation_high);
    gParameterSuite->paramGetValueAtTime(myData->saturationLowSoftnessParam, time, &saturation_low_softness);
    gParameterSuite->paramGetValueAtTime(myData->saturationHighSoftnessParam, time, &saturation_high_softness);
    gParameterSuite->paramGetValueAtTime(myData->luminanceLowParam, time, &luminance_low);
    gParameterSuite->paramGetValueAtTime(myData->luminanceHighParam, time, &luminance_high);
    gParameterSuite->paramGetValueAtTime(myData->luminanceLowSoftnessParam, time, &luminance_low_softness);
    gParameterSuite->paramGetValueAtTime(myData->luminanceHighSoftnessParam, time, &luminance_high_softness);

    printf("99\n");

    // the property sets holding our images
    OfxPropertySetHandle outputImg = NULL, sourceImg = NULL;
    try {
      // fetch image to render into from that clip
      OfxPropertySetHandle outputImg;
      if(gImageEffectSuite->clipGetImage(myData->outputClip, time, NULL, &outputImg) != kOfxStatOK) {
        throw " no output image!";
      }

      // fetch image at render time from that clip
      if (gImageEffectSuite->clipGetImage(myData->sourceClip, time, NULL, &sourceImg) != kOfxStatOK) {
        throw " no source image!";
      }

      // figure out the data types
      char *cstr;
      gPropertySuite->propGetString(outputImg, kOfxImageEffectPropComponents, 0, &cstr);
      std::string components = cstr;

      // how many components per pixel?
      int nComps = 0;
      if(components == kOfxImageComponentRGBA) {
        nComps = 4;
      }
      else if(components == kOfxImageComponentRGB) {
        nComps = 3;
      }
      else if(components == kOfxImageComponentAlpha) {
        nComps = 1;
      }
      else {
        throw " bad pixel type!";
      }
      printf("50\n");

      // now do our render depending on the data type
      gPropertySuite->propGetString(outputImg, kOfxImageEffectPropPixelDepth, 0, &cstr);
      std::string dataType = cstr;

      if(dataType == kOfxBitDepthByte) {
        PixelProcessing<unsigned char, 255>(hue_enabled, hue, hue_width, hue_softness,
                                            saturation_enabled, saturation_low, saturation_high, saturation_low_softness, saturation_high_softness,
                                            luminance_enabled, luminance_low, luminance_high, luminance_low_softness, luminance_high_softness,
                                            instance, sourceImg, outputImg, renderWindow, nComps);
      }
      else if(dataType == kOfxBitDepthShort) {
        PixelProcessing<unsigned short, 65535>(hue_enabled, hue, hue_width, hue_softness,
                                               saturation_enabled, saturation_low, saturation_high, saturation_low_softness, saturation_high_softness,
                                               luminance_enabled, luminance_low, luminance_high, luminance_low_softness, luminance_high_softness,
                                               instance, sourceImg, outputImg, renderWindow, nComps);
      }
      else if (dataType == kOfxBitDepthFloat) {
        PixelProcessing<float, 1>(hue_enabled, hue, hue_width, hue_softness,
                                  saturation_enabled, saturation_low, saturation_high, saturation_low_softness, saturation_high_softness,
                                  luminance_enabled, luminance_low, luminance_high, luminance_low_softness, luminance_high_softness,
                                  instance, sourceImg, outputImg, renderWindow, nComps);
      }
      else {
        throw " bad data type!";
        throw 1;
      }

    }
    catch(const char *errStr ) {
      bool isAborting = gImageEffectSuite->abort(instance);

      // if we were interrupted, the failed fetch is fine, just return kOfxStatOK
      // otherwise, something wierd happened
      if(!isAborting) {
        status = kOfxStatFailed;
      }
      ERROR_IF(!isAborting, " Rendering failed because %s", errStr);

    }

    if(sourceImg)
      gImageEffectSuite->clipReleaseImage(sourceImg);
    if(outputImg)
      gImageEffectSuite->clipReleaseImage(outputImg);

    printf("Render end\n");
    // all was well
    return status;
  }

  // are the settings of the effect making it redundant and so not do anything to the image data
  OfxStatus IsIdentityAction( OfxImageEffectHandle instance,
                              OfxPropertySetHandle inArgs,
                              OfxPropertySetHandle outArgs)
  {
    return kOfxStatReplyDefault;
    MyInstanceData *myData = FetchInstanceData(instance);

    double time;
    gPropertySuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

    double hue = 1.0;
    gParameterSuite->paramGetValueAtTime(myData->hueParam, time, &hue);

    // if the hue value is 1.0 (or nearly so) say we aren't doing anything
    if(fabs(hue - 1.0) < 0.000000001) {
      // we set the name of the input clip to pull default images from
      gPropertySuite->propSetString(outArgs, kOfxPropName, 0, "Source");
      // and say we trapped the action and we are at the identity
      return kOfxStatOK;
    }

    // say we aren't at the identity
    return kOfxStatReplyDefault;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Call back passed to the host in the OfxPlugin struct to set our host pointer
  //
  // This must be called AFTER both OfxGetNumberOfPlugins and OfxGetPlugin, but
  // before the pluginMain entry point is ever touched.
  void SetHostFunc(OfxHost *hostStruct)
  {
    gHost = hostStruct;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // The main entry point function, the host calls this to get the plugin to do things.
  OfxStatus MainEntryPoint(const char *action, const void *handle, OfxPropertySetHandle inArgs,  OfxPropertySetHandle outArgs)
  {
    printf("action is : %s \n", action );
    // cast to appropriate type
    OfxImageEffectHandle effect = (OfxImageEffectHandle) handle;

    OfxStatus returnStatus = kOfxStatReplyDefault;

    if(strcmp(action, kOfxActionLoad) == 0) {
      // The very first action called on a plugin.
      returnStatus = LoadAction();
    }
    else if(strcmp(action, kOfxActionDescribe) == 0) {
      // the first action called to describe what the plugin does
      returnStatus = DescribeAction(effect);
    }
    else if(strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
      // the second action called to describe what the plugin does in a specific context
      returnStatus = DescribeInContextAction(effect, inArgs);
    }
    else if(strcmp(action, kOfxActionCreateInstance) == 0) {
      // the action called when an instance of a plugin is created
      returnStatus = CreateInstanceAction(effect);
    }
    else if(strcmp(action, kOfxActionDestroyInstance) == 0) {
      // the action called when an instance of a plugin is destroyed
      returnStatus = DestroyInstanceAction(effect);
    }
    else if(strcmp(action, kOfxImageEffectActionIsIdentity) == 0) {
      // Check to see if our param settings cause nothing to happen
      returnStatus = IsIdentityAction(effect, inArgs, outArgs);
    }
    else if(strcmp(action, kOfxImageEffectActionRender) == 0) {
      // action called to render a frame
      returnStatus = RenderAction(effect, inArgs, outArgs);
    }
    else {
      printf("unhandled\n");
    }

    MESSAGE(": END action is : %s \n", action );
    /// other actions to take the default value
    return returnStatus;
  }

} // end of anonymous namespace


////////////////////////////////////////////////////////////////////////////////
// The plugin struct passed back to the host application to initiate bootstrapping
// of plugin communications
static OfxPlugin effectPluginStruct =
{
  kOfxImageEffectPluginApi,                // The API this plugin satisfies.
  1,                                       // The version of the API it satisifes.
  "joeboy.qualiflower",                    // The unique ID of this plugin.
  1,                                       // The major version number of this plugin.
  0,                                       // The minor version number of this plugin.
  SetHostFunc,                             // Function used to pass back to the plugin the OFXHost struct.
  MainEntryPoint                           // The main entry point to the plugin where all actions are passed to.
};

////////////////////////////////////////////////////////////////////////////////
// The first of the two functions that a host application will look for
// after loading the binary, this function returns the number of plugins within
// this binary.
//
// This will be the first function called by the host.
EXPORT int OfxGetNumberOfPlugins(void)
{
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// The second of the two functions that a host application will look for
// after loading the binary, this function returns the 'nth' plugin declared in
// this binary.
//
// This will be called multiple times by the host, once for each plugin present.
EXPORT OfxPlugin * OfxGetPlugin(int nth)
{
  if(nth == 0)
    return &effectPluginStruct;
  return 0;
}
