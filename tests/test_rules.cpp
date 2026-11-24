#include <cassert>
#include <iostream>
#include "../rules.h"
int tests=0;
void check(bool ok) { ++tests; assert(ok); }
Decision tick(RiskEngine& e, Sample& s, bool armed=false) {
  s.at+=2000; return e.update(s,armed,s.at);
}
int main() {
  {
    RiskEngine e; Sample s;
    check(tick(e,s).severity==NORMAL); // R10
    s.flame=true;
    auto d=tick(e,s); check(d.severity==ALERT && (d.pendingMask & (1u<<2)));
    d=tick(e,s); check(d.ruleMask & (1u<<2)); // R03
    check(!e.reset(s.at));
    s.flame=false; tick(e,s); tick(e,s); check(!e.reset(s.at));
    tick(e,s); check(e.result.severity==ALERT); check(e.reset(s.at));
    check(e.result.severity==NORMAL);
  }
  {
    RiskEngine e; Sample s; s.temperature=60; s.mq=2500;
    auto d=tick(e,s); check(d.severity==ALERT && !d.relay);
    d=tick(e,s); check(d.severity==CRITICAL && d.relay && (d.ruleMask&(1u<<1))); // R02 sans flamme
    s.flame=true; d=tick(e,s); check(d.severity==CRITICAL);
    d=tick(e,s); check(d.ruleMask&1u); // R01
    s.temperatureValid=false; d=tick(e,s); check(!d.healthy && d.relay && d.severity==CRITICAL);
    check(!e.reset(s.at));
  }
  {
    RiskEngine e; Sample s; s.mq=2500;
    auto d=tick(e,s); check(d.active[ATMOSPHERIC]==ALERT);
    d=tick(e,s); check((d.ruleMask&(1u<<3)) && d.severity==ALERT); // R04
    s.temperature=45; d=tick(e,s); d=tick(e,s);
    check(d.displayed[FIRE]==WARNING && d.displayed[ATMOSPHERIC]==ALERT);
  }
  {
    RiskEngine e; Sample s; s.temperature=60;
    check(tick(e,s).severity==WARNING);
    auto d=tick(e,s); check(d.severity==ALERT && (d.ruleMask&(1u<<4))); // R05
  }
  {
    RiskEngine e; Sample s; s.temperature=44.9;
    check(tick(e,s).severity==NORMAL);
    s.temperature=45; tick(e,s); auto d=tick(e,s);
    check(d.severity==WARNING && (d.ruleMask&(1u<<5))); // R06
    s.temperature=43; d=tick(e,s); check(d.severity==WARNING); // hystérésis
    s.temperature=25; tick(e,s); tick(e,s); check(e.result.severity==WARNING);
    check(tick(e,s).severity==NORMAL); // trois acquisitions sous le retour
  }
  {
    RiskEngine e; Sample s; s.mq=1499;
    check(tick(e,s).severity==NORMAL);
    s.mq=1500; tick(e,s); check(tick(e,s).displayed[ATMOSPHERIC]==WARNING);
  }
  {
    RiskEngine e; Sample s; s.door=true; s.motion=true;
    check(tick(e,s,false).severity==NORMAL); // désarmé
    tick(e,s,true); auto d=tick(e,s,true);
    check(d.severity==ALERT && (d.ruleMask&(1u<<6))); // R07
    d=tick(e,s,false); check(d.severity==ALERT); // désarmer n'efface pas une alarme
  }
  {
    RiskEngine e; Sample s; s.door=true;
    tick(e,s,true); auto d=tick(e,s,true);
    check(d.severity==WARNING && (d.ruleMask&(1u<<7))); // R08
    s.door=false; s.at+=6000; e.update(s,true,s.at);
    s.motion=true; d=tick(e,s,true); d=tick(e,s,true);
    check(d.severity==WARNING); // porte ancienne exclue de la fenêtre
  }
  {
    RiskEngine e; Sample s; s.source=false;
    tick(e,s); auto d=tick(e,s);
    check(d.severity==WARNING && (d.ruleMask&(1u<<8))); // R09
    s.source=true; tick(e,s); tick(e,s); check(tick(e,s).severity==NORMAL);
  }
  {
    RiskEngine e; Sample s; s.temperatureValid=false;
    auto d=tick(e,s); check(!d.healthy && d.severity==WARNING && !(d.ruleMask&(1u<<9)));
    s.mq=3000; d=tick(e,s); check(d.severity==ALERT && !d.healthy); // MQ2 reste utile sans DHT
  }
  {
    RiskEngine e; Sample s; s.mqReady=false;
    check(!tick(e,s).healthy);
    s.flame=true; check(tick(e,s).severity==ALERT); // alarme forte pendant préchauffage
  }
  {
    RiskEngine e; Sample s; tick(e,s);
    auto d=e.update(s,false,s.at+6001);
    check(!d.healthy && d.severity==WARNING);
    check(!e.reset(s.at+6001));
  }
  {
    RiskEngine e; Sample s; s.temperature=65; s.mq=3000;
    tick(e,s); auto d=e.update(s,false,s.at);
    check(d.severity==ALERT); // doublon ne confirme pas R02
    s.at+=1000; d=e.update(s,false,s.at); check(d.severity==ALERT); // trop rapproché
    s.at+=1000; d=e.update(s,false,s.at); check(d.severity==CRITICAL);
  }
  {
    RiskEngine e; Sample s; s.temperature=65; s.mq=3000;
    tick(e,s); s.at+=8000; auto d=e.update(s,false,s.at);
    check(d.severity==ALERT); // trou d'acquisition : recommencer confirmation
  }
  {
    RiskEngine e; Sample s; s.temperature=65; s.mq=3000; s.source=false; s.door=true; s.motion=true;
    tick(e,s,true); auto d=tick(e,s,true);
    check(d.displayed[FIRE]==CRITICAL && d.displayed[INTRUSION]==ALERT && d.displayed[POWER]==WARNING);
  }
  std::cout << tests << " assertions : OK (regles, limites, maintien, defauts, temps).\n";
}
