RTAI
====

This is a clone of RTAI from https://www.rtai.org.  **The master branch of this
repository has many additional features and bug fixes that are not supported
by Paolo Mantegazza (the main developer and owner of RTAI).**  However, I try
to keep branches named after the RTAI modules up-to-date with their development
in the original CVS ropository at https://gna.org/cvs/?group=rtai.

I have also added the showroom of RTAI in this repository.

This repository does not keep track of RTAI's history before version 3.9, which
is the initial commit of this repository.  Currently, the `vulcano` branch tracks
any changes to the `vulcano` module of the original CVS repository, as well
as containing its `showroom` module.  This branch will always be merged into
`master`, so that `master` would contain the latest RTAI development, plus many
other changes.
