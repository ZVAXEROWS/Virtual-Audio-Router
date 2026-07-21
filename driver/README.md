# driver/ — Future Virtual Audio Driver

This directory is reserved for the **Phase 8** Windows kernel-mode or user-mode virtual audio driver.

## Plan

The virtual audio driver will implement the `IDevice` interface defined in `engine/include/var/IDevice.h`,
allowing `AudioRouter` to treat the virtual device exactly like any real WASAPI endpoint.

### Driver options under evaluation

| Option | Type | Pros | Cons |
|---|---|---|---|
| WASAPI Virtual Device (APO) | User-mode | No signing required | Limited control |
| Windows Audio Service Plugin | User-mode | Easy, no WDK | Restricted |
| WDM/KS Audio Driver | Kernel-mode | Full control, lowest latency | Requires EV certificate, WDK |
| PortCls miniport | Kernel-mode | Industry standard | Complex, requires WDK |

### References

- [Windows Driver Kit (WDK) Docs](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/)
- [VB-Audio virtual driver approach](https://vb-audio.com/Cable/)
- [Virtual Cable architecture (PortCls)](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/portcls-support-by-operating-system)
