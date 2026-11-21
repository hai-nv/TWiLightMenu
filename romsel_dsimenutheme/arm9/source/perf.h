#ifndef PERF_H
#define PERF_H

// Diagnostic timing marks (temporary). perfMark() only stores a tick count and a label in RAM;
// perfFlush() appends everything to sd:/_nds/TWiLightMenu/perf.txt in one write.
#include <nds.h>
#include <stdio.h>
#include <time.h>

struct PerfData {
	u32 t[96];
	const char *label[96];
	int n;
	u32 last;
	bool started;
};

inline PerfData &perfData() {
	static PerfData d;
	return d;
}

inline void perfMark(const char *label) {
	PerfData &d = perfData();
	if (!d.started) {
		cpuStartTiming(0);
		d.started = true;
	}
	const u32 now = cpuGetTiming();
	if (d.n == 0) d.last = now; // first mark of a batch: delta 0
	if (d.n < 96) {
		d.t[d.n] = now;
		d.label[d.n] = label;
		d.n++;
	}
}

inline void perfFlush(const char *title) {
	PerfData &d = perfData();
	if (d.n == 0) return;
	FILE *f = fopen("sd:/_nds/TWiLightMenu/perf.txt", "ab");
	if (f) {
		fprintf(f, "== %s (rtc %lu) ==\r\n", title, (unsigned long)time(NULL));
		u32 prev = d.t[0];
		const u32 first = d.t[0];
		for (int i = 0; i < d.n; i++) {
			// 33.514 ticks per microsecond-thousandth: ticks / 33514 = ms
			fprintf(f, "  +%6lu ms  (%5lu ms)  %s\r\n", (unsigned long)((d.t[i] - first) / 33514), (unsigned long)((d.t[i] - prev) / 33514), d.label[i]);
			prev = d.t[i];
		}
		fclose(f);
	}
	d.n = 0; // the next mark starts a new batch
}

#endif