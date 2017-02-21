# About remote-control #
[![Travis build][travis-badge]][travis]

This repository contains _remote-control_, a middleware for Avionic Design's
Medcom Terminals, Linux-based entertainment and communication devices
targeted for the healthcare sector.

The _remote-control_ software uses WebKitGTK to load and display web-based user
interfaces. By expanding the included JavaScriptCore with its own
`AvionicDesign` object, various hardware and many low-level software functions
can be controlled via JavaScript. This currently includes (but is not limited
to) Voice-over-IP using Linphone, video streamer via VLC or GStreamer,
controlling and mixing various audio sources and sinks, display brightness
control and various input events from internal and external devices, e.g. a
handset.

## Name ##

The name _remote-control_ nowadays is a bit misleading as it really is more
like _local-control_, exposing a JavaScript extension for the device it runs on
and all. However, before v1.0.0, _remote-control_ exposed an RPC API a service
on the server it connected to could control various hardware functions. This
design dates way back to a time when Medcom devices were not Linux-based but
used Windows CE instead. After switching from daylight openings to penguins,
the RPC interface was gradually converted to a JavaScript extension for
performance reasons. Especially in large networks saving the round-trip to the
server for simple operations like changing the backlight brightness led to a
noticeable speed increase.

Since _remote-control_ v1.0.0 the conversion to JS is complete for every
function required on current and future devices, dropping the dated RPC API in
the progress. The name of the middleware may be changed in the future -
developers often like to stick to the labels they assign to things, so it might
as well never happen ;)

# Building #

This project is based on Autotools, so building is mostly executing
`autogen.sh` with the desired configuration options. _remote-control_ works
with both WebKitGTK 1 and 2 versions. For the latter, compiling against GTK 3
is mandatory. Apart from the usual dependencies like Autotools and things
building things, internally developed libraries are available from our
[public FTP][adftp]:

* [gtkosk](http://ftp.avionic-design.de/pub/gtkosk/)
* [libgpio](http://ftp.avionic-design.de/pub/libgpio/)
* [libsmartcard](http://ftp.avionic-design.de/pub/libsmartcard/)

# Installation #

Building and installation into an embedded Linux image can be done smoothly
using [PBS][pbs2] with the [AD platform][pbsad], but should also work
independently. Considering its near exclusive use on Medcom devices, usefulness
in other environments is unconfirmed but might exist.

# Documentation #

Documentation is admittedly a bit scarce and outdated, but can be found in the
docs/ subdirectory. We are hoping to improve the situation in the future.

# Development #

Issues may be reported via [GitHub][bugs-github]. Pull requests are welcome,
bear in mind though we might have different plans or use cases.

  [adftp]: http://ftp.avionic-design.de/pub/ "Avionic Design public FTP server"
  [pbs2]: https://github.com/avionic-design/pbs-stage2 "Platform Build System"
  [pbsad]: https://github.com/avionic-design/pbs-platform-avionic-design
    "AD platform for PBS"
  [bugs-github]: https://github.com/avionic-design/remote-control/issues
  [travis]: https://travis-ci.org/avionic-design/remote-control
  [travis-badge]: https://travis-ci.org/avionic-design/remote-control.svg?branch=master

