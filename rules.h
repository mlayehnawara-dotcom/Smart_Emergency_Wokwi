#pragma once
#include <stdint.h>
#include <math.h>

// Version locale 3.1 : seuils bruts fictifs, sans concentration en ppm.
// UNKNOWN vaut -1 pour conserver les comparaisons du sketch existant.
enum Severity { UNKNOWN = -1, NORMAL = 0, WARNING, ALERT, CRITICAL };
enum Incident { FIRE, ATMOSPHERIC, INTRUSION, POWER, INCIDENT_COUNT };
enum DataHealth { INITIALIZING, DATA_OK, DEGRADED, OFFLINE };

inline const char* severityName(int s) {
  switch (s) {
    case NORMAL: return "NORMAL";
    case WARNING: return "WARNING";
    case ALERT: return "ALERT";
    case CRITICAL: return "CRITICAL";
    default: return "UNKNOWN";
  }
}
inline const char* incidentName(int i) {
  const char* names[] = {"FIRE", "ATMOSPHERIC", "INTRUSION", "POWER"};
  return i >= 0 && i < INCIDENT_COUNT ? names[i] : "UNKNOWN";
}
inline const char* healthName(int h) {
  switch (h) {
    case DATA_OK: return "OK";
    case DEGRADED: return "DEGRADED";
    case OFFLINE: return "OFFLINE";
    default: return "INITIALIZING";
  }
}

struct Sample {
  uint32_t at = 0;
  float temperature = 25, humidity = 45;
  int mq = 0;
  bool temperatureValid = true, mqReady = true;
  bool flame = false, motion = false, door = false, source = true;
};
struct Decision {
  int active[INCIDENT_COUNT] = {};
  int displayed[INCIDENT_COUNT] = {};
  unsigned ruleMask = 0, pendingMask = 0;
  int severity = UNKNOWN;
  int health = INITIALIZING;
  bool healthy = false, relay = false, newAlarm = false;
};

class RiskEngine {
  unsigned streak[9] = {};
  uint32_t countedAt[9] = {};
  int latched[INCIDENT_COUNT] = {};
  unsigned safeFrames = 0;
  uint32_t safeAt = 0, lastSample = 0, lastReception = 0;
  bool haveSample = false;

  void clearConfirmation() {
    for (int i = 0; i < 9; ++i) streak[i] = 0;
    safeFrames = 0;
  }
  void unavailable(int health) {
    clearConfirmation();
    result.healthy = false;
    result.health = health;
    result.severity = UNKNOWN;
    result.ruleMask = result.pendingMask = 0;
    result.newAlarm = false;
    for (int i = 0; i < INCIDENT_COUNT; ++i) {
      result.active[i] = NORMAL;
      result.displayed[i] = latched[i];
      if (latched[i] >= ALERT && latched[i] > result.severity)
        result.severity = latched[i];
    }
    result.relay = latched[FIRE] == CRITICAL;
  }

public:
  Decision result;
  static constexpr int Q_WARN = 1200, Q_HIGH = 2000;
  static constexpr uint32_t PERIOD = 2000, MAX_CONFIRM_GAP = 3000;
  static constexpr uint32_t FRESH_MS = 6000, OFFLINE_MS = 90000;

  // Signature conservée pour sketch.ino. L'intrusion utilise désormais
  // le mouvement ET la porte présents dans la même acquisition.
  void observe(bool, bool, bool, uint32_t) {}

  unsigned normalFrames() const { return safeFrames; }

  // À appeler régulièrement dans loop() pour surveiller une perte de données.
  void poll(uint32_t now) {
    if (!haveSample) return;
    uint32_t age = uint32_t(now - lastReception);
    if (age > FRESH_MS)
      unavailable(age > OFFLINE_MS ? OFFLINE : DEGRADED);
  }

  Decision update(const Sample& s, bool armed, uint32_t now) {
    poll(now);
    int32_t age = static_cast<int32_t>(now - s.at);
    if (age > static_cast<int32_t>(FRESH_MS) || age < -2000) {
      unavailable(result.health == OFFLINE ? OFFLINE : DEGRADED);
      return result;
    }
    if (haveSample && s.at == lastSample) {
      result.newAlarm = false;
      return result; // Doublon : aucun compteur ni date rafraîchis.
    }
    if (haveSample && static_cast<int32_t>(s.at - lastSample) < 0) {
      unavailable(DEGRADED);
      return result;
    }
    if (haveSample && uint32_t(s.at - lastSample) > MAX_CONFIRM_GAP)
      clearConfirmation();
    haveSample = true;
    lastSample = s.at;
    lastReception = now;

    const bool tempOK = s.temperatureValid && isfinite(s.temperature) &&
      isfinite(s.humidity) && s.humidity >= 0 && s.humidity <= 100;
    const bool mqOK = s.mqReady && s.mq >= 0 && s.mq <= 4095;
    const bool tempHigh = tempOK && s.temperature >= 60;
    const bool mqHigh = mqOK && s.mq >= Q_HIGH;
    const bool critical = tempHigh && mqHigh;

    // R01 incendie ; R02 flamme ; R03 MQ élevé ; R04 température élevée ;
    // R05 température modérée ; R06 MQ modéré ; R07 intrusion combinée ;
    // R08 intrusion isolée ; R09 source absente ; R10 situation normale.
    const bool conditions[9] = {
      critical, s.flame, mqHigh && !critical, tempHigh && !critical,
      tempOK && s.temperature >= 45 && s.temperature < 60,
      mqOK && s.mq >= Q_WARN && s.mq < Q_HIGH,
      armed && s.motion && s.door,
      armed && (s.motion != s.door), !s.source
    };
    const int types[9] = {
      FIRE, FIRE, ATMOSPHERIC, FIRE, FIRE, ATMOSPHERIC,
      INTRUSION, INTRUSION, POWER
    };
    const int levels[9] = {
      CRITICAL, ALERT, ALERT, ALERT, WARNING, WARNING,
      ALERT, WARNING, WARNING
    };
    const unsigned required[9] = {2, 1, 1, 1, 2, 2, 1, 2, 2};

    Decision d;
    d.healthy = tempOK && mqOK;
    d.health = d.healthy ? DATA_OK : DEGRADED;
    d.severity = NORMAL;
    bool safe = d.healthy;
    for (int i = 0; i < 9; ++i) {
      if (!conditions[i]) { streak[i] = 0; continue; }
      safe = false;
      if (streak[i] == 0 || uint32_t(s.at - countedAt[i]) > MAX_CONFIRM_GAP) {
        streak[i] = 1;
        countedAt[i] = s.at;
      } else if (uint32_t(s.at - countedAt[i]) >= PERIOD) {
        if (streak[i] < 2) ++streak[i];
        countedAt[i] = s.at;
      }
      int level = NORMAL;
      if (streak[i] >= required[i]) {
        d.ruleMask |= 1u << i;
        level = levels[i];
      } else {
        d.pendingMask |= 1u << i;
        if (i == 0) level = ALERT; // Incendie : ALERTE dès la première trame.
      }
      if (level > d.active[types[i]]) d.active[types[i]] = level;
    }

    if (!safe) safeFrames = 0;
    else if (safeFrames == 0 || uint32_t(s.at - safeAt) > MAX_CONFIRM_GAP) {
      safeFrames = 1;
      safeAt = s.at;
    } else if (uint32_t(s.at - safeAt) >= PERIOD) {
      if (safeFrames < 3) ++safeFrames;
      safeAt = s.at;
    }

    for (int i = 0; i < INCIDENT_COUNT; ++i) {
      if (d.active[i] >= ALERT && d.active[i] > latched[i]) {
        latched[i] = d.active[i];
        d.newAlarm = true;
      }
      d.displayed[i] = latched[i] > d.active[i] ? latched[i] : d.active[i];
      if (d.displayed[i] > d.severity) d.severity = d.displayed[i];
    }
    if (!d.healthy && d.severity < ALERT) d.severity = UNKNOWN;
    if (safe && d.severity == NORMAL) d.ruleMask |= 1u << 9;
    d.relay = latched[FIRE] == CRITICAL;
    result = d;
    return result;
  }

  bool reset(uint32_t now) {
    poll(now);
    if (!haveSample || uint32_t(now - lastReception) > FRESH_MS ||
        !result.healthy || safeFrames < 3) return false;
    for (int i = 0; i < INCIDENT_COUNT; ++i) {
      latched[i] = NORMAL;
      result.displayed[i] = result.active[i] = NORMAL;
    }
    result.severity = NORMAL;
    result.relay = false;
    result.newAlarm = false;
    result.ruleMask = 1u << 9;
    result.pendingMask = 0;
    for (int i = 0; i < 9; ++i) streak[i] = 0;
    return true;
  }
};
