#  Kext Self-diagnostics

## Goals


In the past we've seen hangs that have been very difficult to diagnose. It would be great if there were some command(s) that we could issue to the kext to get more information about its current state (e.g. is it blocked waiting on a reply from VFS4G?).

## Technical background

Hangs in a kext usually have an adverse effect on overall system stability, particularly kexts with kauth listeners, as virtually every process in the system will trigger callbacks in the kext.
