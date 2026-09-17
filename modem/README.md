# Modem (MSS) on the Xperia Z2

The modem works on mainline once two Sony-specific things are in place.

**1. Sony's TA services.** Early in its init the modem firmware asks the
application processor, over QMI services 227/228/229, for units of the TA
(trim area) partition — unit 2227, 2551, 2550 and 2209 on this phone. Without
an answer the init task stalls and the modem's own watchdog resets it about
40 s after boot ("dog.c: Watchdog detects stalled initialization"), before it
ever touches EFS or registers a telephony service. Bjorn Andersson's
`ta-service` (github.com/andersson/ta-service, packaged in pmaports) provides
those services, but as of commit 594fd47a it parses only the first block of
the partition that carries the TA magic and stops; on this phone the units the
modem wants are in the second block, so it answered "0 bytes" and the modem
sat idle. `ta-service/0001-ta-load-every-block-of-the-TA-partition.patch`
parses every block (later blocks take precedence, as a journal should).
`ta-service/ta-service.service` starts it before rmtfs, which starts the modem.

**2. Where the telephony QMI services are.** On msm8974 the modem serves
DMS/NAS/UIM/WDS over its SMD QMUX channel, which the kernel exposes as
`/dev/wwan0qmi0` (with AT on `/dev/wwan0at0`), not over IPC router. So
`qrtr-lookup` never lists them, and that is not a fault. `qmicli -d
/dev/wwan0qmi0 --dms-get-ids` returns the IMEI; ModemManager's `qcom-soc`
plugin claims the ports. `msm-modem` and `msm-modem-uim-selection` from
pmaports select the SIM slot. Verified 2026-09-15: modem online, firmware
MPSS.DI.2.0.1.c1.9, `ATI` → SONY D6503 23.5.A.1.291; ModemManager reports
`sim-missing` with no SIM fitted, which is the correct state.

`qmi-honeypot.c` publishes a range of QMI service IDs on the AP and logs which
one a passive modem contacts — it showed this modem waits for nothing beyond
the TA services.

**GPS.** The GNSS engine is in the modem and works: `qmicli -d /dev/wwan0qmi0
-p --loc-start --client-no-release-cid`, then `--client-cid=N
--client-no-release-cid --loc-follow-nmea`, streams NMEA at 1 Hz (GGA, RMC,
GSA, VTG, GSV). Indoors it listed 16 satellites with no signal and no fix;
a fix needs sky view, and XTRA assistance needs a data connection. Over IPC
router (`qrtr://0`) each qmicli call is its own client and the session ends
with it, so use the SMD port. ModemManager reports location capabilities
gps-nmea/gps-raw/agps and the Sony SUPL server, for geoclue.

**TA units and ta-service gaps** (from the stock binaries and dosomder's
ta-info): 2209 = build type, 2227 = startup/shutdown result (Sony's libmiscta
also writes it), 2551 = baseband configuration id, 2550 = unknown. The stock
`ta_qmi_service` serves 7 TA messages (open, close, read, write, delete,
getsize, getnextid) on 227 and 5 MiscTA messages on 228; `ta-service` answers
only open/close/read/iterate and get_size/read, replies nothing to the rest,
and nothing on this phone serves 230 (`mlog_qmi_service`, the modem's log
sink). The modem initialises regardless; add no-op replies if a later step
turns out to block on a write.

**That step turned up.** The modem sends message 4 on service 228 at the end
of every answered call and waits for a reply. Ignoring it starves the modem's
non-volatile storage task, and a couple of minutes later its own watchdog
kills the firmware with `dog.c:1639:Watchdog detects task starvation of nve`,
after which it reloads, re-enumerates with a new ModemManager index, and
refuses incoming calls in between. `ta-service/0002-*.patch` answers it with
a success result and writes nothing, which is enough to let the task finish;
what the message actually asks for is still unknown, and guessing at a write
into the trim area would be worse than not writing, since that partition
holds the radio calibration.

Two things worth knowing if you go near this. The modem re-enumerates with a
new index after a restart, so nothing should hardcode `-m 0`. And asking this
modem for a low power state (`mmcli -m N --set-power-state-low`) crashes it
rather than turning the radio off, because a wedged task cannot service the
deactivate; use `--reset`.
