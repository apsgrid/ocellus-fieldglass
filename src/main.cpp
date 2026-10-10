#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "esp_system.h"
#include "esp_bt.h"
#include "esp_log.h"
#include "board_e32r40t.h"
#include "companion_location.h"
#include <time.h>

#define FG_VERSION "0.3.0-companion"

TFT_eSPI tft;
SPIClass sdSpi(HSPI);
Preferences fgPrefs;
BLEScan* bleScan=nullptr;
File logFile;

static uint16_t BG,PANEL,TEXT,MUTED,ACCENT,GOOD,WARN,BAD;
static bool sdReady=false,fsReady=false,recording=true,storageFault=false,safeMode=false;
static uint32_t seqNo=0,loggedEvents=0,sessionStartMs=0;
static uint32_t repeatLogMs=60000UL;
static constexpr size_t FLASH_RESERVE_BYTES=128*1024;
static uint32_t nextWifiAt=0,nextBleAt=0,lastUi=0,lastStatusLog=0,lastFlushMs=0,lastLocationLogMs=0;
static uint16_t dirtyLines=0;
static int wifiSeenNow=0,bleSeenNow=0,wifiUnique=0,bleUnique=0;
static float batteryV=0;
static int batteryPct=-1;
static bool lowPower=false,criticalPower=false,powerProtected=false;
static float batteryMinV=99.0f;
static uint32_t bootCount=0;
static uint8_t faultStreak=0;
static uint32_t pairingPin=0;
static esp_reset_reason_t bootResetReason=ESP_RST_UNKNOWN;
static String storageLabel="NONE";
static String logPath,observerId,sessionId;
static String lastWifiName="-",lastWifiInfo="-",lastBleName="-",lastBleInfo="-";

struct SeenEntry { uint32_t hash=0; uint32_t lastLogged=0; };
static SeenEntry wifiCache[220];
static SeenEntry bleCache[220];

struct BleCycleEntry {
  uint32_t hash=0;
  char addr[18]{};
  char name[32]{};
  int8_t rssi=-127;
  int8_t txPower=127;
  uint16_t manufacturerLen=0;
};
static constexpr int BLE_CYCLE_CAP=160;
static BleCycleEntry bleCycle[BLE_CYCLE_CAP];
static volatile int bleCycleCount=0;
static volatile uint32_t blePacketsThisScan=0;
static volatile uint32_t bleDroppedThisScan=0;

uint32_t fnv1a(const char* s){
  uint32_t h=2166136261u;
  while(*s){ h^=(uint8_t)*s++; h*=16777619u; }
  return h?h:1;
}
uint32_t fnv1a(const String& s){ return fnv1a(s.c_str()); }

bool shouldLog(SeenEntry* cache,int cap,uint32_t h,uint32_t now,int& uniqueCount){
  int freeSlot=-1,oldest=0;
  uint32_t oldestAt=UINT32_MAX;
  for(int i=0;i<cap;i++){
    if(cache[i].hash==h){
      if(now-cache[i].lastLogged>=repeatLogMs){ cache[i].lastLogged=now; return true; }
      return false;
    }
    if(cache[i].hash==0 && freeSlot<0) freeSlot=i;
    if(cache[i].lastLogged<oldestAt){ oldestAt=cache[i].lastLogged; oldest=i; }
  }
  int slot=(freeSlot>=0)?freeSlot:oldest;
  cache[slot].hash=h; cache[slot].lastLogged=now; uniqueCount++;
  return true;
}

String esc(const String& in){
  String out; out.reserve(in.length()+8);
  for(size_t i=0;i<in.length();i++){
    char c=in[i];
    if(c=='\\' || c=='"'){ out+='\\'; out+=c; }
    else if(c=='\n' || c=='\r' || (uint8_t)c<32) out+=' ';
    else out+=c;
  }
  return out;
}

bool isFaultReset(esp_reset_reason_t r){
  return r==ESP_RST_PANIC || r==ESP_RST_INT_WDT || r==ESP_RST_TASK_WDT || r==ESP_RST_WDT;
}

const char* resetReasonText(esp_reset_reason_t r){
  switch(r){
    case ESP_RST_POWERON:return "power_on";
    case ESP_RST_EXT:return "external";
    case ESP_RST_SW:return "software";
    case ESP_RST_PANIC:return "panic";
    case ESP_RST_INT_WDT:return "interrupt_wdt";
    case ESP_RST_TASK_WDT:return "task_wdt";
    case ESP_RST_WDT:return "watchdog";
    case ESP_RST_DEEPSLEEP:return "deep_sleep";
    case ESP_RST_BROWNOUT:return "brownout";
    case ESP_RST_SDIO:return "sdio";
    default:return "unknown";
  }
}

int batteryPercentFromV(float v){
  if(v>=4.20f)return 100;
  if(v>=4.10f)return 90+(int)((v-4.10f)*100.0f);
  if(v>=4.00f)return 80+(int)((v-4.00f)*100.0f);
  if(v>=3.90f)return 65+(int)((v-3.90f)*150.0f);
  if(v>=3.80f)return 45+(int)((v-3.80f)*200.0f);
  if(v>=3.70f)return 30+(int)((v-3.70f)*150.0f);
  if(v>=3.60f)return 15+(int)((v-3.60f)*150.0f);
  if(v>=3.45f)return 5+(int)((v-3.45f)*66.7f);
  if(v>=3.30f)return (int)((v-3.30f)*33.3f);
  return 0;
}

void sampleBattery(){
  uint64_t sum=0; constexpr int n=24;
  for(int i=0;i<n;i++){ sum+=analogReadMilliVolts(board_e32r40t::BAT_ADC); delayMicroseconds(150); }
  batteryV=((float)sum/n)/1000.0f*board_e32r40t::BAT_DIVIDER;
  batteryPct=(batteryV>2.5f && batteryV<4.5f)?constrain(batteryPercentFromV(batteryV),0,100):-1;
  lowPower=(batteryPct>=0 && batteryV<3.55f);
  criticalPower=(batteryPct>=0 && batteryV<3.38f);
  if(batteryPct>=0 && batteryV<batteryMinV) batteryMinV=batteryV;
}

void flushLog(bool force=false){
  if(!logFile || dirtyLines==0) return;
  uint32_t now=millis();
  if(force || dirtyLines>=8 || now-lastFlushMs>=3000UL){
    logFile.flush();
    dirtyLines=0;
    lastFlushMs=now;
  }
}

void appendLine(const String& line){
  if(!recording || !fsReady || storageFault)return;
  if(!sdReady){
    size_t total=LittleFS.totalBytes();
    size_t used=LittleFS.usedBytes();
    size_t freeBytes=(total>used)?(total-used):0;
    if(total==0 || freeBytes<FLASH_RESERVE_BYTES || line.length()+4096>freeBytes){
      storageFault=true;
      recording=false;
      storageLabel="FLASH FULL";
      if(logFile){ logFile.flush(); logFile.close(); }
      Serial.printf("storage guard: stop logging used=%u total=%u free=%u\\n",(unsigned)used,(unsigned)total,(unsigned)freeBytes);
      return;
    }
  }
  if(!logFile) logFile=sdReady?SD.open(logPath,FILE_APPEND):LittleFS.open(logPath,FILE_APPEND);
  if(logFile){
    size_t n=logFile.println(line);
    if(n){ loggedEvents++; dirtyLines++; flushLog(false); }
    else{
      storageFault=true;
      recording=false;
      storageLabel="FLASH ERROR";
      Serial.println("storage write failed; logging stopped safely");
    }
  }else{
    storageFault=true;
    recording=false;
    storageLabel="FLASH ERROR";
    Serial.println("storage open failed; logging stopped safely");
  }
}

String companionUtcIso(const CompanionLocationSnapshot& fix){
  if(fix.unixTimeS < 1600000000UL) return String();
  uint32_t ageS=(uint32_t)(millis()-fix.receivedMs)/1000UL;
  time_t t=(time_t)(fix.unixTimeS+ageS);
  struct tm utc{};
  if(!gmtime_r(&t,&utc)) return String();
  char buf[24];
  strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%SZ",&utc);
  return String(buf);
}

String eventPrefix(const char* kind){
  uint32_t eventSeq=seqNo++;
  uint32_t nowMs=millis();
  CompanionLocationSnapshot fix;
  companionLocationSnapshot(fix);
  bool hasCompanion=companionLocationFresh(fix,10000UL);
  String utc=hasCompanion?companionUtcIso(fix):String();

  String s; s.reserve(820);
  s+="{\"schema\":\"fieldglass.event/1.0\",\"event_id\":\"";
  s+=sessionId+"-"+String(eventSeq);
  s+="\",\"session_id\":\""+sessionId+"\",\"sequence\":"+String(eventSeq);
  s+=",\"kind\":\""+String(kind)+"\",\"observer\":{\"id\":\""+observerId+"\",\"hardware\":\"lcdwiki-e32r40t\",\"firmware\":\""+String(FG_VERSION)+"\"},";

  if(utc.length()){
    s+="\"time\":{\"utc\":\""+utc+"\",\"monotonic_ms\":"+String(nowMs)+",\"quality\":\"companion\"},";
  }else{
    s+="\"time\":{\"utc\":null,\"monotonic_ms\":"+String(nowMs)+",\"quality\":\"unknown\"},";
  }

  if(hasCompanion){
    s+="\"location\":{\"lat\":"+String(fix.lat,7)+",\"lon\":"+String(fix.lon,7);
    s+=",\"accuracy_m\":"+String(fix.accuracyM,1);
    if(fix.hasSpeed) s+=",\"speed_mps\":"+String(fix.speedMps,2);
    else s+=",\"speed_mps\":null";
    if(fix.hasBearing) s+=",\"heading_deg\":"+String(fix.bearingDeg,2);
    else s+=",\"heading_deg\":null";
    s+=",\"source\":\"companion\"},";
    s+="\"privacy\":{\"mode\":\"raw\",\"location\":\"precise\",\"ssid_redacted\":false},";
    s+="\"provenance\":{\"location_source\":\"companion\",\"location_authenticated\":true,\"location_mock\":";
    s+=(fix.mock?String("true"):String("false"));
    s+="},";
  }else{
    s+="\"location\":null,\"privacy\":{\"mode\":\"raw\",\"location\":\"suppressed\",\"ssid_redacted\":false},";
  }
  return s;
}

void logSession(const char* kind){
  String line=eventPrefix(kind);
  line+="\"payload\":{\"firmware\":\""+String(FG_VERSION)+"\",\"profile\":\"WALK\",";
  line+="\"boot_count\":"+String(bootCount)+",\"fault_streak\":"+String(faultStreak)+",\"reset_reason\":\""+String(resetReasonText(bootResetReason))+"\",";
  line+="\"battery_v\":"+String(batteryV,3)+",\"free_heap\":"+String(ESP.getFreeHeap())+",\"min_free_heap\":"+String(ESP.getMinFreeHeap())+"}}";
  appendLine(line);
}

void logStatus(){
  CompanionLocationSnapshot fix;
  companionLocationSnapshot(fix);
  bool locFresh=companionLocationFresh(fix,10000UL);

  String line=eventPrefix("system.status");
  line+="\"payload\":{\"battery_v\":"+String(batteryV,3)+",\"battery_pct\":"+String(batteryPct)+",";
  line+="\"free_heap\":"+String(ESP.getFreeHeap())+",\"min_free_heap\":"+String(ESP.getMinFreeHeap())+",";
  line+="\"wifi_unique\":"+String(wifiUnique)+",\"ble_unique\":"+String(bleUnique)+",\"storage\":\""+storageLabel+"\"";
  if(!sdReady && fsReady){
    line+=",\"flash_used\":"+String((unsigned)LittleFS.usedBytes())+",\"flash_total\":"+String((unsigned)LittleFS.totalBytes());
  }
  line+=",\"battery_min_v\":"+String(batteryMinV,3);
  line+=",\"companion_connected\":"+(fix.connected?String("true"):String("false"));
  line+=",\"companion_authenticated\":"+(fix.authenticated?String("true"):String("false"));
  line+=",\"companion_fix_fresh\":"+(locFresh?String("true"):String("false"));
  line+="}}";
  appendLine(line);
}

void logCompanionFixIfNeeded(){
  CompanionLocationSnapshot fix;
  if(!companionLocationTakeDirty(fix)) return;
  if(!companionLocationFresh(fix,10000UL)) return;

  uint32_t interval=sdReady?1000UL:5000UL;
  uint32_t now=millis();
  if(now-lastLocationLogMs<interval) return;
  lastLocationLogMs=now;

  String line=eventPrefix("location.fix");
  line+="\"payload\":{\"source\":\"companion\",\"transport\":\"ble_gatt\",";
  line+="\"authenticated\":"+(fix.authenticated?String("true"):String("false"))+",";
  line+="\"mock\":"+(fix.mock?String("true"):String("false"))+",";
  line+="\"accuracy_m\":"+String(fix.accuracyM,1)+"}}";
  appendLine(line);
}

String makeObserverId(){
  uint64_t id=ESP.getEfuseMac();
  char buf[32];
  snprintf(buf,sizeof(buf),"esp32-%08lx%08lx",(unsigned long)(id>>32),(unsigned long)(id&0xffffffffULL));
  return String(buf);
}

void setupStorage(){
  sdSpi.begin(board_e32r40t::SD_SCLK,board_e32r40t::SD_MISO,board_e32r40t::SD_MOSI,board_e32r40t::SD_CS);
  // No-card is a normal operating mode; suppress the SD driver's expected error spam.
  esp_log_level_set("sd_diskio",ESP_LOG_NONE);
  sdReady=SD.begin(board_e32r40t::SD_CS,sdSpi,12000000);
  esp_log_level_set("sd_diskio",ESP_LOG_ERROR);
  if(!sdReady){
    SD.end();
    sdSpi.end();
    Serial.println("SD not present; using onboard flash");
  }
  if(sdReady){ fsReady=true; storageLabel="SD"; repeatLogMs=60000UL; }
  else {
    // Never auto-format after a mount failure: field data is more valuable than convenience.
    fsReady=LittleFS.begin(false);
    storageLabel=fsReady?"FLASH":"FLASH BAD";
    repeatLogMs=300000UL;
    if(!fsReady) storageFault=true;
  }
  logPath="/fieldglass-"+String((uint32_t)ESP.getEfuseMac(),HEX)+"-b"+String(bootCount)+".jsonl";
  if(fsReady){
    logFile=sdReady?SD.open(logPath,FILE_WRITE):LittleFS.open(logPath,FILE_WRITE);
    if(logFile) logFile.close();
  }
}

void setupTouch(){
  uint16_t cal[5];
  const char* key="touchcal";
  size_t n=fgPrefs.getBytesLength(key);
  if(n==sizeof(cal)){
    fgPrefs.getBytes(key,cal,sizeof(cal));
  }else{
    // One-time migration from the old Jellyfin-remote calibration namespace.
    Preferences legacy;
    legacy.begin("apsjr3",true);
    size_t legacyLen=legacy.getBytesLength("tcalP3");
    if(legacyLen==sizeof(cal)){
      legacy.getBytes("tcalP3",cal,sizeof(cal));
      fgPrefs.putBytes(key,cal,sizeof(cal));
    }else{
      tft.fillScreen(BG);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TEXT,BG); tft.drawString("Touch calibration",160,190,4);
      tft.setTextColor(MUTED,BG); tft.drawString("Tap each marker",160,225,2);
      tft.calibrateTouch(cal,ACCENT,BG,15);
      fgPrefs.putBytes(key,cal,sizeof(cal));
    }
    legacy.end();
  }
  tft.setTouch(cal);
}

void drawBar(int y,const String& left,const String& right,uint16_t rc=0){
  tft.fillRect(12,y,296,36,PANEL);
  tft.setTextDatum(ML_DATUM); tft.setTextColor(MUTED,PANEL); tft.drawString(left,22,y+18,2);
  tft.setTextDatum(MR_DATUM); tft.setTextColor(rc?rc:TEXT,PANEL); tft.drawString(right,298,y+18,2);
}

void drawUi(){
  tft.fillScreen(BG);
  tft.fillRect(0,0,320,52,PANEL);
  tft.setTextDatum(TL_DATUM); tft.setTextColor(TEXT,PANEL); tft.drawString("OCELLUS",10,6,2);
  tft.setTextColor(ACCENT,PANEL); tft.drawString("FIELDGLASS",10,24,2);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(batteryPct<0?MUTED:(batteryPct<20?WARN:GOOD),PANEL);
  tft.drawString(batteryPct<0?"BAT --":String(batteryPct)+"%",310,16,2);

  tft.setTextDatum(TL_DATUM);
  const char* state=safeMode?"SAFE MODE":(storageFault?"STORAGE ERROR":(powerProtected?"LOW BATTERY":(recording?"RECORDING":"PAUSED")));
  tft.setTextColor((safeMode||storageFault||powerProtected)?BAD:(recording?GOOD:WARN),BG);
  tft.drawString(state,14,65,4);
  tft.setTextDatum(TR_DATUM); tft.setTextColor(MUTED,BG);
  uint32_t secs=(millis()-sessionStartMs)/1000;
  char dur[16]; snprintf(dur,sizeof(dur),"%02lu:%02lu",(unsigned long)(secs/60),(unsigned long)(secs%60));
  tft.drawString(dur,306,72,2);

  drawBar(104,"Wi-Fi now",String(wifiSeenNow)+"   unique "+String(wifiUnique));
  drawBar(144,"BLE now",String(bleSeenNow)+"   unique "+String(bleUnique));
  drawBar(184,"Logged",String(loggedEvents));
  String storageUi=storageLabel;
  if(!sdReady && fsReady){
    size_t total=LittleFS.totalBytes(),used=LittleFS.usedBytes();
    storageUi+=" "+String((unsigned)((total>used?total-used:0)/1024))+"K";
  }
  drawBar(224,"Storage",storageUi,(fsReady&&!storageFault)?GOOD:BAD);

  CompanionLocationSnapshot loc;
  companionLocationSnapshot(loc);
  String locUi;
  uint16_t locColor=MUTED;
  if(companionLocationFresh(loc,10000UL)){
    locUi=String("PHONE ±")+String(loc.accuracyM,1)+"m";
    locColor=GOOD;
  }else if(loc.connected && loc.authenticated){
    locUi="PHONE WAIT";
    locColor=WARN;
  }else if(loc.connected){
    locUi="PAIR "+String(companionLocationPairingPin());
    locColor=WARN;
  }else{
    locUi="PAIR "+String(companionLocationPairingPin());
  }
  drawBar(264,"Location",locUi,locColor);

  tft.setTextDatum(TL_DATUM); tft.setTextColor(MUTED,BG); tft.drawString("LAST WI-FI",14,310,1);
  tft.setTextColor(TEXT,BG); tft.drawString(lastWifiName.substring(0,28),14,326,2);
  tft.setTextColor(MUTED,BG); tft.drawString(lastWifiInfo.substring(0,40),14,348,1);

  tft.setTextColor(MUTED,BG); tft.drawString("LAST BLE",14,366,1);
  tft.setTextColor(TEXT,BG); tft.drawString(lastBleName.substring(0,28),14,382,2);
  tft.setTextColor(MUTED,BG); tft.drawString(lastBleInfo.substring(0,40),14,404,1);

  uint16_t bc=(safeMode||storageFault||powerProtected)?WARN:(recording?BAD:GOOD);
  tft.fillRoundRect(12,420,296,48,10,bc);
  tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_BLACK,bc);
  const char* button=safeMode?"SAFE MODE":(storageFault?"STORAGE LOCKED":(powerProtected?"POWER PROTECTED":(recording?"STOP SESSION":"START SESSION")));
  tft.drawString(button,160,444,2);
}

class FieldglassBleCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    blePacketsThisScan++;
    std::string a=d.getAddress().toString();
    uint32_t h=fnv1a(a.c_str());
    int slot=-1;
    for(int i=0;i<bleCycleCount;i++){
      if(bleCycle[i].hash==h){ slot=i; break; }
    }
    if(slot<0){
      int count=bleCycleCount;
      if(count>=BLE_CYCLE_CAP){ bleDroppedThisScan++; return; }
      slot=count;
      bleCycleCount=count+1;
      bleCycle[slot]=BleCycleEntry{};
      bleCycle[slot].hash=h;
    }
    strlcpy(bleCycle[slot].addr,a.c_str(),sizeof(bleCycle[slot].addr));
    if(d.haveName()) strlcpy(bleCycle[slot].name,d.getName().c_str(),sizeof(bleCycle[slot].name));
    else strlcpy(bleCycle[slot].name,"<unnamed>",sizeof(bleCycle[slot].name));
    bleCycle[slot].rssi=(int8_t)d.getRSSI();
    bleCycle[slot].txPower=d.haveTXPower()?(int8_t)d.getTXPower():127;
    bleCycle[slot].manufacturerLen=d.haveManufacturerData()?(uint16_t)d.getManufacturerData().size():0;
  }
};
FieldglassBleCallbacks bleCallbacks;

void scanWifi(){
  // Passive survey: do not transmit probe requests while walking.
  int n=WiFi.scanNetworks(false,true,true,180);
  wifiSeenNow=max(0,n);
  uint32_t now=millis();
  for(int i=0;i<n;i++){
    String bssid=WiFi.BSSIDstr(i),ssid=WiFi.SSID(i);
    int32_t rssi=WiFi.RSSI(i),ch=WiFi.channel(i);
    lastWifiName=ssid.length()?ssid:"<hidden>";
    lastWifiInfo=bssid+"  ch "+String(ch)+"  "+String(rssi)+" dBm";
    uint32_t h=fnv1a(bssid);
    if(shouldLog(wifiCache,220,h,now,wifiUnique)){
      String line=eventPrefix("wifi.ap"); line.reserve(760);
      line+="\"signal\":{\"rssi_dbm\":"+String(rssi)+",\"channel\":"+String(ch)+"},";
      line+="\"payload\":{\"bssid\":\""+bssid+"\",\"ssid\":\""+esc(ssid)+"\",\"auth_mode\":"+String((int)WiFi.encryptionType(i))+"}}";
      appendLine(line);
    }
  }
  WiFi.scanDelete();
  Serial.printf("wifi n=%d unique=%d heap=%u min=%u bat=%.3fV\n",wifiSeenNow,wifiUnique,ESP.getFreeHeap(),ESP.getMinFreeHeap(),batteryV);
}

void scanBle(){
  if(!bleScan)return;

  // The classic Arduino BLE stack can leave a synchronous scan waiting for
  // its GAP completion event while this device is advertising. Time-slice the
  // single BLE radio explicitly: pause discoverability for the short survey
  // window, then resume it if no companion is already connected.
  CompanionLocationSnapshot companionBefore;
  companionLocationSnapshot(companionBefore);
  bool companionConnected=companionBefore.connected;

  BLEDevice::stopAdvertising();
  delay(40);

  bleCycleCount=0; blePacketsThisScan=0; bleDroppedThisScan=0;
  bleScan->start(1,false);

  if(!companionConnected){
    delay(20);
    BLEDevice::startAdvertising();
  }

  bleSeenNow=bleCycleCount;
  uint32_t now=millis();
  for(int i=0;i<bleCycleCount;i++){
    const BleCycleEntry& d=bleCycle[i];
    lastBleName=String(d.name);
    lastBleInfo=String(d.addr)+"  "+String((int)d.rssi)+" dBm";
    if(shouldLog(bleCache,220,d.hash,now,bleUnique)){
      String line=eventPrefix("ble.advertisement"); line.reserve(720);
      line+="\"signal\":{\"rssi_dbm\":"+String((int)d.rssi)+"},";
      line+="\"payload\":{\"address\":\""+String(d.addr)+"\",\"name\":\""+esc(String(d.name))+"\"";
      if(d.manufacturerLen) line+=",\"manufacturer_len\":"+String(d.manufacturerLen);
      if(d.txPower!=127) line+=",\"tx_power\":"+String((int)d.txPower);
      line+="}}";
      appendLine(line);
    }
  }
  bleScan->clearResults();
  Serial.printf("ble uniq-now=%d packets=%lu dropped=%lu unique=%d heap=%u min=%u bat=%.3fV\n",
    bleSeenNow,(unsigned long)blePacketsThisScan,(unsigned long)bleDroppedThisScan,bleUnique,
    ESP.getFreeHeap(),ESP.getMinFreeHeap(),batteryV);
}

fs::FS* activeStore(){
  if(sdReady) return &SD;
  if(fsReady) return &LittleFS;
  return nullptr;
}

void stopSession(){
  if(!recording) return;
  logSession("session.end");
  flushLog(true);
  recording=false;
  if(logFile) logFile.close();
}

bool startSession(){
  if(safeMode || storageFault || powerProtected || !fsReady) return false;
  recording=true;
  sessionStartMs=millis();
  sessionId=observerId+"-b"+String(bootCount)+"-s"+String(sessionStartMs);
  seqNo=0;
  logSession("session.start");
  flushLog(true);
  uint32_t now=millis();
  nextWifiAt=now+500;
  nextBleAt=now+5000;
  return true;
}

void handlePowerProtection(){
  static uint8_t criticalSamples=0;
  static uint8_t recoverySamples=0;

  if(criticalPower){
    recoverySamples=0;
    if(criticalSamples<10) criticalSamples++;
    if(criticalSamples>=3 && !powerProtected){
      stopSession();
      powerProtected=true;
      Serial.printf("power protection engaged at %.3fV\n",batteryV);
      drawUi();
    }
    return;
  }

  criticalSamples=0;
  if(powerProtected && batteryPct>=0 && batteryV>=3.65f){
    if(recoverySamples<10) recoverySamples++;
    if(recoverySamples>=3){
      powerProtected=false;
      recoverySamples=0;
      Serial.printf("power recovered at %.3fV; session remains stopped\n",batteryV);
      drawUi();
    }
  }else{
    recoverySamples=0;
  }
}

void printRuntimeStatus(){
  size_t total=0,used=0;
  if(!sdReady && fsReady){
    total=LittleFS.totalBytes();
    used=LittleFS.usedBytes();
  }
  CompanionLocationSnapshot fix;
  companionLocationSnapshot(fix);
  bool locFresh=companionLocationFresh(fix,10000UL);
  Serial.printf(
    "STATUS fw=%s boot=%lu reset=%s safe=%d faults=%u recording=%d storage=%s fault=%d "
    "battery=%.3fV min=%.3fV pct=%d heap=%u minheap=%u "
    "wifi_unique=%d ble_unique=%d logged=%lu flash_used=%u flash_total=%u "
    "companion_connected=%d companion_auth=%d companion_fix=%d pair=%06lu\n",
    FG_VERSION,(unsigned long)bootCount,resetReasonText(bootResetReason),safeMode?1:0,(unsigned)faultStreak,
    recording?1:0,storageLabel.c_str(),storageFault?1:0,
    batteryV,batteryMinV,batteryPct,ESP.getFreeHeap(),ESP.getMinFreeHeap(),
    wifiUnique,bleUnique,(unsigned long)loggedEvents,(unsigned)used,(unsigned)total,
    fix.connected?1:0,fix.authenticated?1:0,locFresh?1:0,(unsigned long)companionLocationPairingPin());
}

void listStorageFiles(){
  fs::FS* store=activeStore();
  if(!store){ Serial.println("FILES unavailable: no mounted storage"); return; }
  File root=store->open("/");
  if(!root){ Serial.println("FILES unavailable: root open failed"); return; }
  Serial.println("FILES BEGIN");
  File f=root.openNextFile();
  while(f){
    if(!f.isDirectory()) Serial.printf("FILE %s %u\n",f.name(),(unsigned)f.size());
    f.close();
    f=root.openNextFile();
  }
  root.close();
  Serial.println("FILES END");
}

void dumpStorageFile(String path){
  fs::FS* store=activeStore();
  if(!store){ Serial.println("DUMP unavailable: no mounted storage"); return; }
  path.trim();
  if(path.indexOf("..")>=0){ Serial.println("DUMP rejected"); return; }
  if(!path.startsWith("/")) path="/"+path;
  flushLog(true);
  File f=store->open(path,FILE_READ);
  if(!f){ Serial.println("DUMP file not found"); return; }
  Serial.printf("DUMP BEGIN %s %u\n",path.c_str(),(unsigned)f.size());
  uint8_t buf[256];
  while(f.available()){
    size_t n=f.read(buf,sizeof(buf));
    if(n) Serial.write(buf,n);
    delay(0);
  }
  f.close();
  Serial.printf("\nDUMP END %s\n",path.c_str());
}

void processSerialCommand(String cmd){
  cmd.trim();
  if(!cmd.length()) return;
  String upper=cmd;
  upper.toUpperCase();

  if(upper=="STATUS") printRuntimeStatus();
  else if(upper=="FILES") listStorageFiles();
  else if(upper=="STOP"){ stopSession(); drawUi(); Serial.println("OK stopped"); }
  else if(upper=="START"){
    bool ok=startSession();
    drawUi();
    Serial.println(ok?"OK started":"ERR start blocked");
  }
  else if(upper.startsWith("DUMP ")) dumpStorageFile(cmd.substring(5));
  else if(upper=="FORMAT FLASH CONFIRM"){
    stopSession();
    if(sdReady){ Serial.println("ERR SD is active; refusing flash format"); return; }
    Serial.println("FORMAT flash requested");
    LittleFS.end();
    bool ok=LittleFS.format();
    Serial.println(ok?"FORMAT OK; restarting":"FORMAT FAILED");
    delay(100);
    if(ok) ESP.restart();
  }
  else if(upper=="SAFE CLEAR"){
    fgPrefs.putUChar("faults",0);
    Serial.println("SAFE state cleared; restarting");
    delay(100);
    ESP.restart();
  }
  else if(upper=="HELP"){
    Serial.println("COMMANDS: STATUS | FILES | DUMP <path> | STOP | START | SAFE CLEAR | FORMAT FLASH CONFIRM");
  }
  else Serial.println("ERR unknown command; type HELP");
}

void handleSerial(){
  static char buf[128];
  static size_t len=0;
  while(Serial.available()){
    char c=(char)Serial.read();
    if(c=='\r') continue;
    if(c=='\n'){
      buf[len]='\0';
      processSerialCommand(String(buf));
      len=0;
    }else if(len<sizeof(buf)-1){
      buf[len++]=c;
    }else{
      len=0;
      Serial.println("ERR command too long");
    }
  }
}

void checkTouch(){
  static bool wasDown=false;
  static uint16_t lastY=0;
  uint16_t x=0,y=0;
  bool down=tft.getTouch(&x,&y);
  if(down) lastY=y;
  if(!down && wasDown && lastY>=410){
    if(!safeMode && !storageFault && !powerProtected){
      if(recording) stopSession();
      else startSession();
    }
    drawUi();
  }
  wasDown=down;
}

void setup(){
  Serial.begin(115200); delay(200);
  bootResetReason=esp_reset_reason();

  pinMode(board_e32r40t::PIN_TFT_BL,OUTPUT);
  digitalWrite(board_e32r40t::PIN_TFT_BL,HIGH);
  pinMode(board_e32r40t::BAT_ADC,INPUT);
  analogReadResolution(12); analogSetPinAttenuation(board_e32r40t::BAT_ADC,ADC_11db);

  tft.init(); tft.setRotation(0); tft.setSwapBytes(true);
  BG=tft.color565(7,10,14); PANEL=tft.color565(18,24,31); TEXT=TFT_WHITE;
  MUTED=tft.color565(145,155,170); ACCENT=tft.color565(52,190,230);
  GOOD=tft.color565(65,220,140); WARN=tft.color565(255,185,70); BAD=tft.color565(255,85,85);

  fgPrefs.begin("fieldglass",false);
  bootCount=fgPrefs.getUInt("boots",0)+1;
  fgPrefs.putUInt("boots",bootCount);

  uint8_t previousFaults=fgPrefs.getUChar("faults",0);
  faultStreak=isFaultReset(bootResetReason)?(uint8_t)min(255,(int)previousFaults+1):0;
  fgPrefs.putUChar("faults",faultStreak);
  safeMode=faultStreak>=3;

  observerId=makeObserverId();
  pairingPin=100000UL+(esp_random()%900000UL);
  setupTouch();
  setupStorage();
  sampleBattery();

  if(!safeMode){
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA); delay(100); WiFi.disconnect(false,true);

    // WALK is BLE-only on the Bluetooth controller. Releasing Classic RAM gives
    // the survey engine materially more heap; CLASSIC will be a separate profile.
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    String bleName="Fieldglass-"+observerId.substring(observerId.length()-4);
    companionLocationInit(bleName,pairingPin);
    bleScan=BLEDevice::getScan();
    bleScan->setActiveScan(false);
    bleScan->setInterval(120);
    bleScan->setWindow(75);
    bleScan->setAdvertisedDeviceCallbacks(&bleCallbacks,true,true);
  }else{
    Serial.printf("SAFE MODE: %u consecutive panic/watchdog resets; radios disabled\n",(unsigned)faultStreak);
  }

  recording=false;
  if(!safeMode && fsReady && !storageFault) startSession();
  drawUi();

  uint32_t now=millis();
  lastUi=now;
  lastStatusLog=now;

  Serial.printf("Ocellus Fieldglass %s boot=%lu reset=%s faults=%u safe=%d storage=%s log=%s heap=%u min=%u battery=%.3fV\n",
    FG_VERSION,(unsigned long)bootCount,resetReasonText(bootResetReason),(unsigned)faultStreak,safeMode?1:0,
    storageLabel.c_str(),logPath.c_str(),ESP.getFreeHeap(),ESP.getMinFreeHeap(),batteryV);
}

void loop(){
  handleSerial();
  checkTouch();
  uint32_t now=millis();

  if(now-lastUi>=5000UL){
    lastUi=now;
    sampleBattery();
    handlePowerProtection();
    flushLog(false);
    drawUi();
  }

  if(recording && now-lastStatusLog>=60000UL){
    lastStatusLog=now;
    logStatus();
  }

  if(recording) logCompanionFixIfNeeded();

  if(recording && !criticalPower && (int32_t)(now-nextWifiAt)>=0){
    scanWifi();
    sampleBattery();
    handlePowerProtection();
    nextWifiAt=millis()+(lowPower?20000UL:12000UL);
  }

  handleSerial();
  checkTouch();
  now=millis();
  if(recording && !criticalPower && (int32_t)(now-nextBleAt)>=0){
    scanBle();
    sampleBattery();
    handlePowerProtection();
    nextBleAt=millis()+(lowPower?24000UL:12000UL);
  }

  handleSerial();
  flushLog(false);
  delay(20);
}