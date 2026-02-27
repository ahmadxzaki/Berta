
## Truck Mode
```mermaid
---
title: "Tracker sending a ping"
---
graph LR
    tracker[Tracker]
    truck[Truck Unit]

    subgraph Cloud
        service[Asset Tracking Service]
        db[(Database)]
        mqtt[MQTT Broker]
    end

    tracker--LoRa-->truck
    truck--LTE-->mqtt
    mqtt-->service
    service--->db

```
```mermaid
sequenceDiagram
    participant tracker as Tracker
    participant truck as Truck Unit
    participant cloud as Cloud

    tracker->>truck: TrackerPingPkt
    truck->>tracker: TrackerPongPkt

    truck->>cloud: TrackerUpdateMqttMsg
```
### TrackerPingPkt (13 bytes)
Little endian
```mermaid
packet
0-2: "Version"
3-5: "Battery Level"
6: "EMRG" %% Emergency mode
7: "RSV" %% Reserved for future use
8-39: "Tracker ID"
40-71: "Monotonic Counter"
72-103: "CMAC"
```

### TrackerPongPkt (9 bytes)
```mermaid
packet
0-7: "Command Byte (Reset, Sleep, Panic, Ack)"
8-39: "Echo of Counter"
40-71: "CMAC"
```
### TrackerUpdateMqttMsg `/tracker/{trackerId}/truck-mode`
```proto
message TrackerUpdateMqttMsg {
    int32 version = 1;
    fixed32 tracker_id = 2;
    fixed32 counter = 3;
    
    int32 battery_level_approx = 4; // 0-15
    bool emergency_mode = 5;

    double truck_latitude = 6;
    double truck_longitude = 7;
    float truck_altitude = 8;
    
    int32 rssi = 9;
    float snr = 10;
    
    google.protobuf.Timestamp timestamp = 11;
}
```

```mermaid
---
title: "Emergency mode"
---
graph LR
    tracker[Tracker]
    gateway[Gateways]

    subgraph Customer Cloud
        mqtt[MQTT Broker]
        service[Asset Tracking Service]
        db[(Database)]
    end

    tracker--LoRa-->gateway
    gateway--WiFi-->mqtt
    mqtt-->service
    service-->db
```
```mermaid
sequenceDiagram
    participant tracker as Tracker
    participant gateways as Gateways
    participant cloud as Cloud

    tracker->>gateways: TrackerPingPkt
    gateways->>cloud: EmergencyTrackerUpdateMqttMsg
```

### TrackerUpdateMqttMsg `/tracker/{trackerId}/emergency-mode`
```proto
message EmergencyTrackerUpdateMqttMsg {
    bytes tracker_ping_payload = 1;
    
    string gateway_id = 2;
    
    uint64 unix_timestamp = 3;
    uint64 timestamp_ns = 4;
    int32 rssi = 5;
    float snr = 6;
    
    double gateway_latitude = 7;
    double gateway_longitude = 8;
    float gateway_altitude = 9;
}
```