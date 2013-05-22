RTAI
====

This is a clone of RTAI from https://www.rtai.org.  **The master branch of this
repository has many additional features and bug fixes that are not supported
by Paolo Mantegazza (the main developer and owner of RTAI).**  However,
I try to keep branches named after the RTAI modules up-to-date with their
development in the original CVS repository at https://gna.org/cvs/?group=rtai.

This repository does not keep track of RTAI's history before
version 3.9, which is the initial commit of this repository. 

Currently, the `vulcano` branch tracks any changes to the `vulcano`
module of the original CVS repository, as well as containing
its `showroom` module, much like `master`.

Please refer to the README.INSTALL file for building instructions.

The `experimental` branch is where the bleeding edge code takes
place. Kernel 3.x support and other large changes will be exclusive
to that branch.
