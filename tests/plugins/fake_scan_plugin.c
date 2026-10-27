/* ════════════════════════════════════════════════════════════════════════
 *  tests/plugins/fake_scan_plugin.c — plugin cảm biến giả, C thuần
 *
 *  Không phần cứng: mỗi poll ngủ 10 ms rồi trả một scan 1081 tia
 *  (−135°…+135°, bước 0.25°), tường tròn 2000 mm + một vật rộng 40 mm ở
 *  (1000, 0) mm — cùng hình học với FakeScipServer của Hokuyo, nên hai
 *  nguồn phải cho ra cùng một blob.
 *
 *  Config: {"fail_after": N}. N > 0: sau N scan thì poll trả −1 với
 *  "fake: cable pulled", để test đường onLost + tự tạo lại.
 *  (Plugin C không có parser JSON; strstr + atoi là đủ cho test.)
 * ════════════════════════════════════════════════════════════════════════ */
#include "mikmap_sensor_plugin.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <windows.h>
static void fake_sleep_ms(int ms) { Sleep((DWORD)ms); }
#else
#  include <unistd.h>
static void fake_sleep_ms(int ms) { usleep((useconds_t)(ms * 1000)); }
#endif

#define FAKE_RAYS 1081
#define FAKE_PI 3.14159265358979323846

typedef struct {
    int  fail_after;
    int  scans;
    char error[128];
} FakeScan;

int mikmap_sensor_api_version(void) {
#ifdef FAKE_API_VERSION
    return FAKE_API_VERSION;
#else
    return MIKMAP_SENSOR_API;
#endif
}

const char* mikmap_sensor_name(void) { return "Fake scan"; }

const char* mikmap_sensor_default_config(void) { return "{\"fail_after\":0}"; }

void* mikmap_sensor_create(const char* configJson) {
    FakeScan* s = (FakeScan*)calloc(1, sizeof(FakeScan));
    if (!s) return NULL;
    if (configJson) {
        const char* p = strstr(configJson, "\"fail_after\"");
        if (p) {
            p = strchr(p, ':');
            if (p) s->fail_after = atoi(p + 1);
        }
    }
    return s;
}

int mikmap_sensor_start(void* handle) {
    FakeScan* s = (FakeScan*)handle;
    if (!s) return -1;
    s->scans = 0;
    s->error[0] = '\0';
    return 0;
}

int mikmap_sensor_poll(void* handle, float* angleRad, float* rangeMm, int maxPoints) {
    FakeScan* s = (FakeScan*)handle;
    const double halfAngle = atan2(20.0, 1000.0);
    int n;
    int i;

    if (!s || !angleRad || !rangeMm || maxPoints <= 0) return -1;

    fake_sleep_ms(10);

    ++s->scans;
    if (s->fail_after > 0 && s->scans > s->fail_after) {
        snprintf(s->error, sizeof s->error, "fake: cable pulled");
        return -1;
    }

    n = FAKE_RAYS < maxPoints ? FAKE_RAYS : maxPoints;
    for (i = 0; i < n; ++i) {
        const double a = (-135.0 + 0.25 * i) * FAKE_PI / 180.0;
        angleRad[i] = (float)a;
        rangeMm[i] = (float)(fabs(a) <= halfAngle ? 1000.0 : 2000.0);
    }
    return n;
}

const char* mikmap_sensor_error(void* handle) {
    FakeScan* s = (FakeScan*)handle;
    return s ? s->error : "no instance";
}

void mikmap_sensor_stop(void* handle) { (void)handle; }

void mikmap_sensor_destroy(void* handle) { free(handle); }
