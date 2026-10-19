#!/usr/bin/env python3
# Замер на живой сессии KDE: отправляет команду работающему Pointshot и каждые 250 мс
# спрашивает у KWin геометрию окон программы.
#
#   tools/dev/kwin-probe.py "debug-move-strip DP-1 left 300" [фильтр]
#
# KWin-скрипт (kwin-geometry.js) возвращает геометрию через callDBus в этот процесс:
# вывод print() из скриптов KWin в журнал не попадает. Нужен python3-dbus.
import dbus, dbus.service, dbus.mainloop.glib, socket, subprocess, sys, time, os
from gi.repository import GLib
dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SessionBus()
name = dbus.service.BusName("org.pointshot.Probe", bus)
filt = sys.argv[2] if len(sys.argv) > 2 else "pointshot"
t0 = None
class Probe(dbus.service.Object):
    @dbus.service.method("org.pointshot.Probe", in_signature="s")
    def Report(self, text):
        lines = [l for l in text.split("\n") if filt.lower() in l.lower()]
        stamp = "before" if t0 is None else f"{time.time()-t0:5.2f}s"
        print(stamp, " | ".join(lines) or "(no match)", flush=True)
Probe(bus, "/")
kwin = bus.get_object("org.kde.KWin", "/Scripting")
def run_script():
    try: kwin.unloadScript("psprobe", dbus_interface="org.kde.kwin.Scripting")
    except Exception: pass
    sid = kwin.loadScript(os.path.join(os.path.dirname(os.path.abspath(__file__)), "kwin-geometry.js"), "psprobe", dbus_interface="org.kde.kwin.Scripting", signature="ss")
    bus.get_object("org.kde.KWin", f"/Scripting/Script{sid}").run(dbus_interface="org.kde.kwin.Script")
    return True
def send():
    global t0
    s = socket.socket(socket.AF_UNIX); s.connect(f"/run/user/{os.getuid()}/io.github.pointshot.Pointshot.sock")
    s.sendall((sys.argv[1] + "\n").encode()); s.close(); t0 = time.time()
    GLib.timeout_add(250, run_script)
    return False
loop = GLib.MainLoop()
run_script()
GLib.timeout_add(600, send)
GLib.timeout_add(5000, loop.quit)
loop.run()
