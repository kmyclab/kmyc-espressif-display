# KMYC controller runtime client

Portable C client for the KMYC Display Expansion Controller ABI major 1 at
7-bit I²C address `0x2C`. ESP-IDF builds can `REQUIRES kmyc_controller`; the core
itself has no ESP-IDF dependency, allocation, device-specific reset sequence or
configuration/Flash write API.

Provide `read(context, register, buffer, length)` and
`write(context, register, buffer, length)` callbacks returning zero on success.
The context owns the I²C device/address. Register reads use a pointer write and
repeated-start. A control write must transmit the register plus all 16 bytes and
complete **STOP**, before a separate response read. The target supports 100 and
400 kHz; allow bounded clock stretching (a 2 ms budget is recommended), rather
than assuming an immediate response. The callbacks must also have finite timeouts.

Call `kmyc_controller_init()`, then `kmyc_controller_probe()`. Probe checks the
signature, protocol major and device type, but deliberately does not pin firmware,
hardware or protocol minor versions. Use `kmyc_controller_is_supported()` to gate
the capabilities required by a selected hardware adapter. Individual commands
also check the capabilities they use. READY/configuration-busy status and adapter
compatibility are policy decisions for the caller, not automatic output actions.

`kmyc_controller_apply()` sends physical GPIO latch/mode masks and optional PWM
duty. GPIO numbers are 0=LCD_RST, 1=TP_RST, 2=TP_INT; modes are 0=unbiased input,
1=push-pull, 2=open-drain. The client assigns no reset polarity, timing or screen
identity to these pins. Release helpers return pins to input; they do not drive a
particular idle level. PWM duty 0 remains actively driven when PWM update is set;
use `release_pwm()` to stop driving. Commands validate response CRC, sequence and
result even when their optional response output is `NULL`.

Serialize **all** operations on each client, including each command write/read
pair, and use a single controlling host. Sequences wrap at 255 and are response
correlation, not a deduplication or security mechanism. The attempted sequence is
stored before the write. A transport failure can mean the command committed but
its acknowledgement was lost: the core does not retry or change outputs on error.
The caller can inspect a fresh response, compare `client.sequence` and result,
and decide recovery. A controller reset or another writer can invalidate that
inference; do not blindly retransmit. `last_transport_error` preserves the latest
callback's native error until another callback runs. `ERR_DEVICE` preserves the
decoded nonzero result. A successful result can have a nonzero historical
`last_error`; only the explicit diagnostics command clears global diagnostics.

## Read-only descriptor and metadata

Identity/status reads cover `0x00–0x2F`; the control response covers `0x40–0x4F`.
Descriptor reads cover the complete **128 bytes at `0x80–0xFF`**, validating both
CRC-32/ISO-HDLC values and the format-1 header. This is the effective descriptor,
not a physical Flash page: the built-in selection is exposed with its effective
payload and active generation, not the zero-length Flash selection record.
Identity `descriptor_source` distinguishes built-in (0) from override (1).
There is no MCU unique identifier in this ABI.

TLV iteration returns binary slices, not NUL-terminated strings. Unknown types
are returned intact and may be skipped. Known type IDs 1–8 describe display ID,
display controller, touch ID, touch controller, dimensions, interface, orientation
and candidate touch addresses. The MCU stores opaque bytes, so valid descriptor
CRCs do not prove valid TLV structure or a physically connected product. The
iterator detects truncated TLVs; product-specific value interpretation belongs
in catalog/adapter code. Touch candidate addresses are not the controller address.

GPIO input fields contain raw MCU input-buffer samples, not a voltage measurement
or reliable interpretation of a floating/1.8 V line. Identity and control response
can sample inputs at different times, even within one register transaction. Read
transactions are individually consistent, but the client does not promise a
cross-transaction snapshot. Reserved response fields and unknown capability/status
bits are not rejected, allowing additive minor-version extensions; the client
always writes zero reserved control bytes. Descriptor format and frame versions
are independently checked.

## Host tests

Run with a C11 compiler, without ESP-IDF or hardware:

```sh
python components/kmyc_controller/tests/run.py --cc cc
# Or: --cc clang / --cc zig cc
```

The runner builds in a temporary directory. Tests cover CRC vectors, field offsets,
control encoding, malformed/corrupted frames, descriptor bounds and TLVs, capability
gates, sequence wrap, optional responses and transport failures without retries.
These are mock-transport tests, not electrical or board compatibility validation.
