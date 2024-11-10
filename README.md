Quick Links: [Installation](#supported-platforms) | [Documentation](#documentation)

[<img align="right" src="https://docs-assets.developer.apple.com/turicreate/turi-dog.svg" alt="Turi Create" width="100">](#)


# XFrame

The XFrame is a scalable column compressed disk-backed dataframe optimized for 
machine learning and data science needs.  It supports for strictly typed
columns (int, float, str, datetime), weakly typed columns (schema free lists,
dictionaries) and has uniform support for missing data.

This is a fork of the SFrame project in Turi Create (originally GraphLab
Create), started by Yucheng Low in the startup GraphLab/Dato/Turi
between 2013 and 2016. Turi was later acquired by Apple in 2016 where we 
open-sourced the project in 2016. Efforts we made to keep it compiling for
several years through heroic efforts by @TobyRoseman, but otherwise minimal 
investments were made.

However, I strongly believe that this is still one of most performant and 
useable data manipulation libraries in Python, and I have wanting to resurrect
this project. However as the name SFrame and Turi Create were taken, we renamed
it from SFrame to XFrame.

Currently, the fork is in an early stage. What has been done:

 - Removed all ML toolkits
 - Removed the SGraph datastructure
 - Renamed SFrame to XFrame
 
But there are many many places for improvement and modernization.

There is a significant amount of technical debt which speaks to the
history of the project. The very original design of the project was a 
client-server model where the client and the server coupld be located on 
different machines. After a while we realized that was not particular useful
and so launched both client and server on the same machine communicating via
IPC (Interprocess Communication). This is the origin of the whole RPC system
called "Unity". Eventually IPC became Inproc, then finally removed, but 
the basic class hierarchy structure remained. 

Following which, there was an effort to build an easy to use C++ interface to
all the datastructures so that people can write extensions/plugins in C++. As
these extensions also needed an easy way to export to Python bindings, we
introduced a whole other class registration mechanism under the "gl_" prefix
(Ex: gl\_sframe, gl\_sarray, etc).

A lot of this can be aggressively simplified and removed.

# Goals 
 - Streamline Python <-> C++ bridge. We currently use Cython, and there is a
 non-trivial amount of Cython. Are there better ways to this today? A way to
 simplify this to use the stable Python APIs might be nice so that we do not
 need to do build for every other python version. 
 - Lambdas currently work by spawning off Python subprocesses and running
Interprocess communication. We could potentially replace this perhaps with 
new PyInterpreters in the same process? Or perhaps even multi-thread now that
Python has a GIL-free interpreter?
 - Native Parquet support in the query engine would be *really* nice.
 - There is a lot of performance we are leaving on the table with vectorization.
We could potentially implement our own, or perhaps consider using Arrow or other
libraries?
 - Others?

# Maintainers
Current maintainers are:
 - Yucheng Low (https://github.com/ylow)
 - Jay Gu (https://github.com/haijieg)


Supported Platforms
-------------------

XFrame current supports only macOS 15+ (Sequoia) because that is what I have.
Linux and Windows should be supported but untestsed.

