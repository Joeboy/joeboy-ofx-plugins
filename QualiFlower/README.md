QualiFlower
===========

This is an effort at implementing something like Davinci Resolve's Qualifier
as an OFX plugin. It works (for me), but here's a list of issues with it:

* It almost certainly won't build on Windows or MacOS. It works for me on
  Ubuntu 21.04.
* It builds against the version of the ofx and ofx support libraries that are
  bundled with Davinci Resolve. They say they're BSD licensed, but I'm not sure
  where else they're available.
* I've only tested it in Davinci Resolve's Fusion tab. It "should" work in
  other hosts, but no guarantees.
* It's CPU or CUDA only. No OpenCL, Metal etc.
* Not sure if I need to do anything to support non-rgba or 24 bit colour images
* Maybe the output should just be an alpha channel rather than RGBA?
* There's no graphical indication of where each hue/saturation/luminance lies
  on the selectors.
* Would be nice to have a colour picker too.
* In Resolve's qualifier, the hue selector goes from magenta to violet. Maybe
  this should work that way too. Having red at the edges is not ideal.

At this point it does what I wanted it to and I have other stuff to do, so
I probably won't be addressing any of the above in the near future unless
anybody tells me they'd like me to.
