# BMW E46 - PROTOCOLO CAN-BUS DETALHADO

**Projeto:** SCG-ECU 2.0 STM32F407VGT6
**Aplicação:** BMW E46 (1999-2006)
**ECU:** Siemens MS43 (330i), MS42 (325i)
**Data:** 2025-11-07

---

## 🔌 CONFIGURAÇÃO FÍSICA DO CAN-BUS

### Especificações do Barramento

```
Protocolo:           ISO 11898 (CAN 2.0B)
Baud Rate:           500 kbps
Tipo:                PT-CAN (Powertrain CAN)
Topologia:           Linear bus (não star)
Terminação:          120Ω em ambas as extremidades
Voltagem:            CAN_H: 2.5-3.5V, CAN_L: 1.5-2.5V (differential)
```

### Conexão Física

**Pinos no Conector OBD-II:**
```
Pin 6:  CAN_H (High) - Yellow/Red wire
Pin 14: CAN_L (Low)  - Yellow/Brown wire
Pin 16: Battery +12V
Pin 4:  Ground
Pin 5:  Ground
```

**Pinos na ECU MS43 (Conector X60002):**
```
Pin 36: CAN_H (High) - Yellow/Red
Pin 37: CAN_L (Low)  - Yellow/Brown
```

### Módulos Conectados ao PT-CAN

```
✅ DME (Engine Control - ECU)
✅ EGS (Automatic Transmission)
✅ KOMBI (Instrument Cluster)
✅ ABS/ASC/DSC (Brake Control)
✅ SZL (Steering Angle Sensor)
✅ AHL (Adaptive Headlight Control) - optional
```

**❌ Módulos NÃO conectados ao PT-CAN:**
- LCM (Light Control) → K-Bus
- GM (General Module) → K-Bus
- CAS (Car Access System) → separate bus
- IHKA (Climate Control) → K-Bus
- PDC (Park Distance) → I-Bus

---

## 📊 MENSAGENS CAN - VISÃO GERAL

### Tabela de Mensagens Principais

| CAN ID | Nome | Sender | Freq | Função |
|--------|------|--------|------|--------|
| **0x153** | ASC1 | ABS/DSC | 10ms | Velocidade, brake status |
| **0x316** | DME1 | DME | 33ms | RPM, torque, status |
| **0x329** | DME2 | DME | 33ms | Coolant, TPS, baro |
| **0x43F** | EGS1 | EGS | 20ms | Gear, trans status |
| **0x545** | DME4 | DME | 100ms | Fuel consumption, CEL, oil |
| **0x613** | ICL2 | KOMBI | 500ms | Odometer |
| **0x615** | ICL3 | KOMBI | 1000ms | Time, date |

---

## 🚗 0x316 - DME1 (RPM E TORQUE)

### Frequência e Prioridade

```
Transmitter:         DME (ECU)
Frequency:           30 Hz (every 33ms)
Priority:            HIGH (critical for dashboard)
Length:              8 bytes
```

### Formato de Bytes

```cpp
Byte 0:  Status bits (DBW, ASC, transmission)
Byte 1:  Indexed torque (% of C_TQ_STND)
Byte 2:  RPM Low byte
Byte 3:  RPM High byte
Byte 4:  Indicated torque after interventions
Byte 5:  Torque loss (friction, AC, alternator)
Byte 6:  Reserved / Not used
Byte 7:  Theoretical torque (before charge intervention)
```

### Byte 0: Status Bits

```
Bit 0:  Terminal 15 ON detected (ignition)
Bit 1:  Reserved
Bit 2:  ASC1 message received and valid (last 500ms)
Bit 3:  Reserved
Bit 4:  Reserved
Bit 5:  Reserved
Bit 6:  Reserved
Bit 7:  Reserved

Typical Value: 0x05 (bits 0 and 2 set)
```

### Bytes 2-3: RPM (Motor Rotation Speed)

**Fórmula de Conversão:**
```cpp
// ECU → CAN:
uint16_t rpm_can = (RPM * 64) / 10;
byte[2] = lowByte(rpm_can);
byte[3] = highByte(rpm_can);

// CAN → RPM:
uint16_t rpm_can = (byte[3] << 8) | byte[2];
float RPM = rpm_can / 6.4;
```

**Exemplos:**
```
RPM = 850  → CAN = 0x0220 (544 dec)
RPM = 3000 → CAN = 0x0780 (1920 dec)
RPM = 6000 → CAN = 0x0F00 (3840 dec)
```

**Range:**
```
Min:  0 RPM      → 0x0000
Max:  10,230 RPM → 0xFFFF (theoretical limit)
```

### Byte 1, 4: Torque Values

**Byte 1: Indexed Torque**
- Torque após todas as intervenções (ASC, DSC)
- Valor: % de C_TQ_STND (torque nominal)
- Range: 0-255 (0-100%+)
- Typical: 0x0C (12%) at idle

**Byte 4: Indicated Torque**
- Torque antes de intervenções externas
- Mesma escala do Byte 1

**Byte 5: Torque Loss**
- Perdas por: atrito, AC compressor, alternador
- Typical: 0x15 (21%) at idle with AC off

**Byte 7: Theoretical Torque**
- Torque teórico do motor (pedal x RPM)
- Typical: 0x35 (53%)

---

## 🌡️ 0x329 - DME2 (TEMPERATURA E TPS)

### Frequência e Prioridade

```
Transmitter:         DME (ECU)
Frequency:           30 Hz (every 33ms)
Priority:            MEDIUM
Length:              8 bytes
```

### Formato de Bytes

```cpp
Byte 0:  Multiplex ID (0x11)
Byte 1:  Coolant Temperature (CLT)
Byte 2:  Barometric Pressure
Byte 3:  Status bits (clutch, engine running)
Byte 4:  TPS Virtual Cruise (não usado E46)
Byte 5:  Throttle Position Sensor (TPS)
Byte 6:  Brake / Kickdown status
Byte 7:  Reserved
```

### Byte 0: Multiplex ID

```
Value:  0x11 (constant)
Purpose: Identifica que esta é a mensagem DME2 padrão
```

### Byte 1: Coolant Temperature (CLT)

**Fórmula de Conversão:**
```cpp
// ECU → CAN:
int temp_can = ((coolant_celsius + 48) * 4) / 3;
if (temp_can > 255) temp_can = 255;  // Clamp
byte[1] = (uint8_t)temp_can;

// CAN → Celsius:
float celsius = (byte[1] * 0.75) - 48.373;
```

**Exemplos:**
```
-40°C  → CAN = 0x0B (11 dec)   → Display: -40°C
0°C    → CAN = 0x40 (64 dec)   → Display: 0°C
20°C   → CAN = 0x5B (91 dec)   → Display: 20°C
90°C   → CAN = 0xB8 (184 dec)  → Display: 90°C
120°C  → CAN = 0xE0 (224 dec)  → Display: 120°C
159°C+ → CAN = 0xFF (255 dec)  → Display: 159°C (max)
```

**Range:**
```
Min:  -48°C  → 0x00
Max:  +159°C → 0xFF
Resolution: 0.75°C per step
```

### Byte 2: Barometric Pressure

**Fórmula de Conversão:**
```cpp
// ECU → CAN:
byte[2] = baro_kPa;  // Direct 1:1 mapping

// CAN → kPa:
float baro_kPa = byte[2];
```

**Exemplos:**
```
Sea Level:   101 kPa → 0x65
Denver:      83 kPa  → 0x53
High Altitude: 75 kPa  → 0x4B
```

**Range:**
```
Min:  0 kPa   → 0x00
Max:  255 kPa → 0xFF
Resolution: 1 kPa per step
```

### Byte 3: Engine Status Bits

```
Bit 0:  Clutch switch (0=pressed, 1=released)
Bit 1:  Reserved
Bit 2:  Reserved
Bit 3:  Engine running (1=running, 0=not running)
Bit 4:  Reserved
Bit 5:  Reserved
Bit 6:  Reserved
Bit 7:  Reserved

Typical Value: 0x08 (engine running, clutch released)
Typical Value: 0x00 (engine off)
```

### Byte 5: Throttle Position Sensor (TPS)

**Fórmula de Conversão:**
```cpp
// ECU → CAN:
// TPS internal format: 0-200 (0-100% em 0.5% steps)
uint8_t tps_can = map(tps_internal, 0, 200, 1, 254);
byte[5] = tps_can;

// CAN → Percentage:
float tps_percent = ((byte[5] - 1) / 253.0) * 100.0;
```

**Exemplos:**
```
Closed Throttle: 0%   → CAN = 0x01 (1 dec)
25% Open:        25%  → CAN = 0x40 (64 dec)
50% Open:        50%  → CAN = 0x7F (127 dec)
WOT:             100% → CAN = 0xFE (254 dec)
```

**Range:**
```
Min:  0x01 (closed)
Max:  0xFE (WOT)
Invalid: 0x00, 0xFF
```

### Byte 6: Brake / Kickdown Status

```
Bit 0:  Brake pedal depressed (1=pressed)
Bit 1:  Reserved
Bit 2:  Kickdown switch (automatic only, 1=pressed)
Bit 3:  Reserved
Bit 4:  Reserved
Bit 5:  Reserved
Bit 6:  Reserved
Bit 7:  Reserved

Typical Value (idle): 0x00
Typical Value (braking): 0x01
Typical Value (kickdown): 0x05
```

---

## ⚠️ 0x545 - DME4 (FUEL CONSUMPTION E WARNING LIGHTS)

### Frequência e Prioridade

```
Transmitter:         DME (ECU)
Frequency:           10 Hz (every 100ms)
Priority:            LOW (non-critical)
Length:              5 bytes
```

### Formato de Bytes

```cpp
Byte 0:  Warning lights (CEL, Cruise, EML)
Byte 1:  Fuel consumption Low byte
Byte 2:  Fuel consumption High byte
Byte 3:  Overheat warning
Byte 4:  Oil temperature
```

### Byte 0: Warning Lights

```
Bit 0:  Reserved
Bit 1:  Check Engine Light (CEL/MIL) - 1=ON
Bit 2:  Reserved
Bit 3:  Cruise Control indicator - 1=ON
Bit 4:  Engine Management Light (EML) - 1=ON
Bit 5:  Reserved
Bit 6:  Reserved
Bit 7:  Reserved

Examples:
0x00 = All lights OFF
0x02 = CEL ON (binary 00000010)
0x10 = EML ON (binary 00010000)
0x08 = Cruise ON (binary 00001000)
0x12 = CEL + EML ON
```

**⚠️ Importante:** Este byte controla as luzes de aviso no painel!
- CEL: Check Engine (falha no motor)
- EML: Engine Management (limitação de potência ativa)

### Bytes 1-2: Fuel Consumption

**Fórmula (não implementada na SCG-ECU ainda):**
```cpp
// Cálculo teórico:
// Consumo (L/h) = (PW × RPM × nCyl × injector_cc) /
//                 (2 × 60,000,000 × densidade)

float consumption_Lh =
  (PW_us * RPM * 6 * 282) / (2.0 * 60000000.0 * 750.0);

uint16_t consumption_can = consumption_Lh * 20.0;  // 0.05 L/h resolution
byte[1] = lowByte(consumption_can);
byte[2] = highByte(consumption_can);

// CAN → L/h:
uint16_t consumption_can = (byte[2] << 8) | byte[1];
float consumption_Lh = consumption_can / 20.0;
```

**Range:**
```
Min:  0.0 L/h    → 0x0000
Max:  3276.75 L/h → 0xFFFF
Resolution: 0.05 L/h per step
```

**Exemplos Típicos:**
```
Idle:         0.8 L/h   → 0x0010 (16 dec)
2000 RPM 20%: 3.5 L/h   → 0x0046 (70 dec)
5000 RPM 80%: 15.0 L/h  → 0x012C (300 dec)
WOT 6000 RPM: 30.0 L/h  → 0x0258 (600 dec)
```

### Byte 3: Overheat Warning

**Fórmula:**
```cpp
if (coolant_celsius > 120) {
  byte[3] = 0x08;  // Overheat light ON
} else {
  byte[3] = 0x00;  // Overheat light OFF
}
```

**Bits:**
```
Bit 0:  Reserved
Bit 1:  Reserved
Bit 2:  Reserved
Bit 3:  Overheat indicator - 1=ON (>120°C)
Bit 4-7: Reserved

Typical: 0x00 (normal operation)
Warning: 0x08 (overheating!)
```

### Byte 4: Oil Temperature

**Fórmula:**
```cpp
// ECU → CAN:
byte[4] = oil_temp_celsius + 48;

// CAN → Celsius:
int oil_temp_celsius = byte[4] - 48;
```

**Exemplos:**
```
Cold:   20°C  → CAN = 0x44 (68 dec)
Normal: 100°C → CAN = 0x94 (148 dec)
Hot:    130°C → CAN = 0xB2 (178 dec)
```

**Range:**
```
Min:  -48°C → 0x00
Max:  +207°C → 0xFF
Typical Operating: 90-110°C
```

---

## 📥 0x153 - ASC1 (VELOCIDADE DO VEÍCULO - ABS envia)

### Frequência e Prioridade

```
Transmitter:         ABS/ASC/DSC Module
Frequency:           100 Hz (every 10ms)
Priority:            HIGH (critical for traction control)
Length:              8 bytes
```

### Formato (Simplificado)

```cpp
Byte 0:  ASC/DSC status
Byte 1:  Vehicle speed Low byte
Byte 2:  Vehicle speed High byte
Byte 3:  Wheel speed FL
Byte 4:  Wheel speed FR
Byte 5:  Wheel speed RL
Byte 6:  Wheel speed RR
Byte 7:  Checksum
```

**⚠️ SCG-ECU não envia ASC1!**
- Esta mensagem é enviada pelo módulo ABS
- SCG-ECU apenas recebe (se implementado)
- Para velocímetro funcionar, ABS precisa estar ativo

---

## 🔄 0x43F - EGS1 (TRANSMISSÃO AUTOMÁTICA)

### Frequência e Prioridade

```
Transmitter:         EGS (Transmission Control)
Frequency:           50 Hz (every 20ms)
Priority:            MEDIUM
Length:              8 bytes
```

### Formato (Simplificado)

```cpp
Byte 0:  Gear position selector (P/R/N/D/S/M)
Byte 1:  Current gear engaged (1-5)
Byte 2:  Transmission temperature
Byte 3-7: Status bits, torque converter, etc.
```

**⚠️ SCG-ECU não interage com EGS!**
- Apenas manual transmission no escopo
- Automática requer engenharia reversa adicional

---

## 📤 IMPLEMENTAÇÃO NA SCG-ECU

### Código Atual (`comms_CAN.cpp`)

**Status de Implementação:**

| Message | Status | Completeness |
|---------|--------|--------------|
| **0x316 (DME1)** | ✅ IMPLEMENTADO | 100% |
| **0x329 (DME2)** | ✅ IMPLEMENTADO | 100% |
| **0x545 (DME4)** | ⚠️ PARCIAL | 70% (falta consumo) |
| **0x153 (ASC1)** | ❌ NÃO IMPLEMENTADO | 0% (RX only) |

### Configuração no TunerStudio

```ini
[CAN_BUS]
Protocol = BMW (option 1)
Baud Rate = 500000
Broadcast Enabled = YES

[CAN_BROADCAST_MSGS]
DME1 (0x316) = 30 Hz
DME2 (0x329) = 30 Hz
DME4 (0x545) = 10 Hz
```

### Pinout SCG-ECU para CAN

```
CAN_H → PD1 (CANTX) - Yellow/Red wire
CAN_L → PD0 (CANRX) - Yellow/Brown wire
```

**⚠️ Termination Resistor:**
- SCG-ECU tem resistor de 120Ω interno? **Verificar!**
- E46 já tem terminação nas extremidades (DME + cluster)
- Se SCG-ECU adicionar 3ª terminação, pode causar problemas

---

## 🧪 TESTE E DIAGNÓSTICO

### Ferramentas Necessárias

1. **OBD-II CAN Adapter**
   - Recomendado: PEAK PCAN-USB
   - Alternativa: ELM327 v1.5+ (verificar se suporta 500kbps)

2. **Software de Monitoramento**
   - SavvyCAN (open-source, excelente)
   - CANalyzer (profissional, pago)
   - BUSMASTER (free, Windows)

### Comando de Teste (Linux com can-utils)

```bash
# Setup interface
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up

# Monitor all messages
candump can0

# Send test DME1 (RPM = 850)
cansend can0 316#050C02200C1500

# Monitor specific ID
candump can0,316:7FF
```

### Sinais Esperados no Painel BMW

**Se tudo OK:**
- ✅ Conta-giros mostra RPM corretamente
- ✅ Temperatura do motor atualiza
- ✅ Luzes CEL/EML respondem a comandos

**Se algo errado:**
- ❌ Conta-giros zerado = CAN não conectado ou baud rate errado
- ❌ Temperatura travada = Mensagem 0x329 não está sendo enviada
- ❌ CEL piscando = Mensagem 0x545 byte[0] com bit incorreto

---

## 📚 REFERÊNCIAS

### Documentação Oficial

- **MS4X Wiki:** https://www.ms4x.net/index.php?title=Siemens_MS43_CAN_Bus
- **MaxxECU BMW E46:** https://www.maxxecu.com/webhelp/can-oem_bmw_e46_330i_ms43.html

### Comunidade

- **E46 Fanatics CAN Thread:** https://www.e46fanatics.com/threads/can-bus-information.734615/
- **Connor's MS43 CAN Guide:** https://mcmillan.website/ms43-can-bus/

### Ferramentas

- **SavvyCAN:** https://github.com/collin80/SavvyCAN
- **can-utils (Linux):** https://github.com/linux-can/can-utils

---

**Última atualização:** 2025-11-07
**Fontes:** MS4X Wiki, E46 Fanatics, Connor McMillan, código SCG-ECU
**Versão:** 1.0
