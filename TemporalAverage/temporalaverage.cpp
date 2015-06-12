#include <string.h>
#include "ofxImageEffect.h"
#include "ofxMemory.h"
#include "ofxMultiThread.h"
#include "ofxPixels.h"

OfxHost               *gHost;
OfxImageEffectSuiteV1 *gEffectHost = 0;
OfxPropertySuiteV1    *gPropHost = 0;

inline OfxRGBAColourB *
pixelAddress(OfxRGBAColourB *img, OfxRectI rect, int x, int y, int bytesPerLine)
{  
  if(x < rect.x1 || x >= rect.x2 || y < rect.y1 || y > rect.y2)
    return 0;
  OfxRGBAColourB *pix = (OfxRGBAColourB *) (((char *) img) + (y - rect.y1) * bytesPerLine);
  pix += x - rect.x1;  
  return pix;
}

class NoImageEx {};

static OfxStatus getFramesNeeded(OfxImageEffectHandle instance,
                                 OfxPropertySetHandle inArgs,
                                 OfxPropertySetHandle outArgs)
{
    OfxTime time;
    double rangeSource[4];
    gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    rangeSource[0] = 1;  // Just because the range apparently needs an even number of elements
    rangeSource[1] = time > 1 ? time - 1 : time;
    rangeSource[2] = time;
    rangeSource[3] = time < 50 ? time + 1 : time;

    gPropHost->propSetDoubleN(outArgs, "OfxImageClipPropFrameRange_Source", 4, rangeSource);
    return kOfxStatOK;
}


static OfxStatus render(OfxImageEffectHandle  instance,
                        OfxPropertySetHandle inArgs,
                        OfxPropertySetHandle outArgs)
{
  OfxTime time;
  OfxRectI renderWindow;
  OfxStatus status = kOfxStatOK;
  
  gPropHost->propGetDouble(inArgs, kOfxPropTime, 0, &time);
  gPropHost->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

  OfxImageClipHandle outputClip;
  gEffectHost->clipGetHandle(instance, "Output", &outputClip, 0);
    

  OfxPropertySetHandle currentImg = NULL, prevImg = NULL, nextImg = NULL, outputImg = NULL;

  try {
    OfxPropertySetHandle outputImg;
    if(gEffectHost->clipGetImage(outputClip, time, NULL, &outputImg) != kOfxStatOK) {
      throw NoImageEx();
    }
      
    int dstRowBytes, dstBitDepth;
    OfxRectI dstRect;
    void *dstPtr;
    gPropHost->propGetInt(outputImg, kOfxImagePropRowBytes, 0, &dstRowBytes);
    gPropHost->propGetIntN(outputImg, kOfxImagePropBounds, 4, &dstRect.x1);
    gPropHost->propGetInt(outputImg, kOfxImagePropRowBytes, 0, &dstRowBytes);
    gPropHost->propGetPointer(outputImg, kOfxImagePropData, 0, &dstPtr);
      
    OfxImageClipHandle sourceClip;
    gEffectHost->clipGetHandle(instance, "Source", &sourceClip, 0);
      
    if(gEffectHost->clipGetImage(sourceClip, time > 1 ? (time - 1) : time, NULL, &prevImg) != kOfxStatOK) {
      throw NoImageEx();
    }
    
    if(gEffectHost->clipGetImage(sourceClip, time < 50 ? (time + 1) : time, NULL, &nextImg) != kOfxStatOK) {
      throw NoImageEx();
    }

    if (gEffectHost->clipGetImage(sourceClip, time, NULL, &currentImg) != kOfxStatOK) {
      throw NoImageEx();
    }
      
    int srcRowBytes, srcBitDepth;
    OfxRectI srcRect;
    void *curPtr, *prevPtr, *nextPtr;
    gPropHost->propGetInt(currentImg, kOfxImagePropRowBytes, 0, &srcRowBytes);
    gPropHost->propGetIntN(currentImg, kOfxImagePropBounds, 4, &srcRect.x1);
    gPropHost->propGetInt(currentImg, kOfxImagePropRowBytes, 0, &srcRowBytes);
    gPropHost->propGetPointer(prevImg, kOfxImagePropData, 0, &prevPtr);
    gPropHost->propGetPointer(currentImg, kOfxImagePropData, 0, &curPtr);
    gPropHost->propGetPointer(nextImg, kOfxImagePropData, 0, &nextPtr);

    OfxRGBAColourB *prev = (OfxRGBAColourB *) prevPtr;
    OfxRGBAColourB *cur = (OfxRGBAColourB *) curPtr;
    OfxRGBAColourB *next = (OfxRGBAColourB *) nextPtr;
    OfxRGBAColourB *dst = (OfxRGBAColourB *) dstPtr;

    for(int y = renderWindow.y1; y < renderWindow.y2; y++) {
      if(gEffectHost->abort(instance)) break;

      OfxRGBAColourB *dstPix = pixelAddress(dst, dstRect, renderWindow.x1, y, dstRowBytes);

      for(int x = renderWindow.x1; x < renderWindow.x2; x++) {
        
        OfxRGBAColourB *prevPix = pixelAddress(prev, srcRect, x, y, srcRowBytes);
        OfxRGBAColourB *curPix = pixelAddress(cur, srcRect, x, y, srcRowBytes);
        OfxRGBAColourB *nextPix = pixelAddress(next, srcRect, x, y, srcRowBytes);

        if(curPix) {
            dstPix->r = (prevPix->r + curPix->r + nextPix->r) / 3;
            dstPix->g = (prevPix->g + curPix->g + nextPix->g) / 3;
            dstPix->b = (prevPix->b + curPix->b + nextPix->b) / 3;
            dstPix->a = 255;
        }
        else {
          dstPix->r = 0;
          dstPix->g = 0;
          dstPix->b = 0;
          dstPix->a = 0;
        }
        dstPix++;
      }
    }
  }
  catch(NoImageEx &) {
    // if we were interrupted, the failed fetch is fine, just return kOfxStatOK
    // otherwise, something wierd happened
    if(!gEffectHost->abort(instance)) {
      status = kOfxStatFailed;
    }      
  }

  if(prevImg)
    gEffectHost->clipReleaseImage(prevImg);
  if(currentImg)
    gEffectHost->clipReleaseImage(currentImg);
  if(outputImg)
    gEffectHost->clipReleaseImage(outputImg);
  
  return status;
}

//  describe the plugin in context
static OfxStatus
describeInContext( OfxImageEffectHandle  effect,  OfxPropertySetHandle inArgs)
{
  OfxPropertySetHandle props;
  // define the single output clip in both contexts
  gEffectHost->clipDefine(effect, "Output", &props);

  // set the component types we can handle on out output
  gPropHost->propSetString(props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);

  // define the single source clip in both contexts
  gEffectHost->clipDefine(effect, "Source", &props);

  // set the component types we can handle on our main input
  gPropHost->propSetString(props, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);

  return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////
// the plugin's description routine
static OfxStatus
describe(OfxImageEffectHandle effect)
{
  // get the property handle for the plugin
  OfxPropertySetHandle effectProps;
  gEffectHost->getPropertySet(effect, &effectProps);

  // say we cannot support multiple pixel depths and let the clip preferences action deal with it all.
  gPropHost->propSetInt(effectProps, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 0);
  
  // set the bit depths the plugin can handle
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthByte);

  // set plugin label and the group it belongs to
  gPropHost->propSetString(effectProps, kOfxPropLabel, 0, "Temporal Denoise");
  gPropHost->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "OFX Example");

  // define the contexts we can be used in
  gPropHost->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);

  gPropHost->propSetInt(effectProps, kOfxImageEffectPropTemporalClipAccess, 0, 1);
  
  return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////
// Called at load
static OfxStatus
onLoad(void)
{
    // fetch the host suites out of the global host pointer
    if(!gHost) return kOfxStatErrMissingHostFeature;
    
    gEffectHost     = (OfxImageEffectSuiteV1 *) gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1);
    gPropHost       = (OfxPropertySuiteV1 *)    gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1);
    if(!gEffectHost || !gPropHost)
        return kOfxStatErrMissingHostFeature;
    return kOfxStatOK;
}

////////////////////////////////////////////////////////////////////////////////
// The main entry point function
static OfxStatus
pluginMain(const char *action,  const void *handle, OfxPropertySetHandle inArgs,  OfxPropertySetHandle outArgs)
{
  // cast to appropriate type
  OfxImageEffectHandle effect = (OfxImageEffectHandle) handle;

  if(strcmp(action, kOfxActionLoad) == 0) {
    return onLoad();
  }
  else if(strcmp(action, kOfxActionDescribe) == 0) {
    return describe(effect);
  }
  else if(strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
    return describeInContext(effect, inArgs);
  }
  else if(strcmp(action, kOfxImageEffectActionRender) == 0) {
    return render(effect, inArgs, outArgs);
  }
  else if(strcmp(action, kOfxImageEffectActionGetFramesNeeded) == 0) {
    return getFramesNeeded(effect, inArgs, outArgs);
  }
    
  return kOfxStatReplyDefault;
}

static void
setHostFunc(OfxHost *hostStruct)
{
  gHost = hostStruct;
}

static OfxPlugin basicPlugin = 
{       
  kOfxImageEffectPluginApi,
  1,
  "joeboy:temporalaverage",
  1,
  0,
  setHostFunc,
  pluginMain
};
   
OfxPlugin *
OfxGetPlugin(int nth)
{
  if(nth == 0)
    return &basicPlugin;
  return 0;
}
 
int
OfxGetNumberOfPlugins(void)
{       
  return 1;
}
