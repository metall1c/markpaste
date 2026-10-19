var out = [];
for (const w of workspace.stackingOrder)
    out.push("[" + w.resourceClass + "|" + w.caption + "] " + w.frameGeometry.x + "," + w.frameGeometry.y + " " + w.frameGeometry.width + "x" + w.frameGeometry.height);
callDBus("org.pointshot.Probe", "/", "org.pointshot.Probe", "Report", out.join("\n"));
