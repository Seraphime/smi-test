# SMI trigger

## What

This exposes a way to trigger SMI via devtmpfs on a specific core.

The module was written against 4.12-rc6, but needs a bit of modification
to the IPI code to make it run.
The easiest way to try it out is to apply the patch in this tree.

## How

To trigger an SMI,

```bash
# echo > /dev/cpu/2/smi # Sends an SMI to cpu 2
# echo -n "current" > /dev/cpu/2/smi # Sends an SMI to the currently running CPU
# dmesg
[...]
[ 2067.616308] smi: Sending SMI IPI to cpu 2 from 3
[ 2068.714095] smi: Sending SMI IPI to cpu 2 from 2
```

## Why

To test a kernel and perf changes claiming to measure SMI cost.
More information about this here:

https://www.spinics.net/lists/kernel/msg2476559.html
https://www.spinics.net/lists/kernel/msg2518285.html
